// report_queue.cpp: the report queue file (see ews/report_queue.h).
// Used by ews-service. Every change reads the whole file, edits the list in memory and writes it back atomically.
// The file is at most 2016 short lines, so this is small and a crash can never leave half a change.
#include "ews/report_queue.h"

#include <windows.h>

#include <algorithm>
#include <filesystem>

#include "ews/atomic_file.h"
#include "ews/file_io.h"

namespace ews {

namespace {

/// The queue as read from the file.
struct QueueState {
    std::vector<std::string> lines;  ///< Valid lines, oldest first.
    QueueRepairs repairs;
    bool exists = false;     ///< The file was there.
    bool oversized = false;  ///< The file was over 16 MB and was not read.
};

/// @return The path of the queue folder.
std::filesystem::path QueueFolder(const std::wstring& dataDir) { return std::filesystem::path(dataDir) / L"queue"; }

/// @return The position of the next line feed at or after `start`, or the text size when there is none.
std::size_t LineEnd(const std::string& text, std::size_t start) { return std::min(text.find('\n', start), text.size()); }

/// Adds one piece of the file to the state. A piece that is not a whole valid line is counted and left out.
void AddSegment(QueueState& state, const std::string& segment, bool complete) {
    bool keep = complete && IsQueueableLine(segment);
    state.repairs.skippedLines += keep ? 0 : 1;
    if (keep) {
        state.lines.push_back(segment);
    }
}

/// Splits the file text into lines. A last piece without a line feed (a crash while writing) is not a line.
void ParseQueueText(const std::string& text, QueueState& state) {
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = LineEnd(text, start);
        AddSegment(state, text.substr(start, end - start), end < text.size());
        start = end + 1;
    }
}

/// Reads the queue file. A missing file is an empty queue; a file over 16 MB is not read and is marked.
/// @return Ok with the state, or the failure when the file exists but cannot be read.
Reading<QueueState> LoadQueue(const std::filesystem::path& file) {
    using Result = Reading<QueueState>;
    Reading<std::string> text = ReadWholeFile(file, kMaxQueueFileBytes);
    QueueState state;
    if (text.status == Status::Unavailable) {
        return Result::Success(state);
    }
    state.exists = true;
    if (text.status == Status::Error && text.error == ERROR_FILE_TOO_LARGE) {
        state.oversized = true;
        state.repairs.brokenFiles = 1;
        return Result::Success(state);
    }
    if (!text.ok()) {
        return Result::Failure(text.status, text.error);
    }
    ParseQueueText(text.value, state);
    return Result::Success(state);
}

/// Moves a queue file that is too large out of the way, so a new queue can start.
Reading<bool> MoveBrokenFile(const std::filesystem::path& folder) {
    return ReplaceByRename(folder / L"reports.jsonl", folder / L"reports.broken.jsonl");
}

/// Drops the oldest lines above the cap. @return How many were dropped.
int TrimToCap(std::vector<std::string>& lines) {
    int excess = std::max(0, static_cast<int>(lines.size()) - kMaxQueueLines);
    lines.erase(lines.begin(), lines.begin() + excess);
    return excess;
}

/// Writes the lines as the new queue file, atomically.
Reading<bool> WriteQueue(const std::filesystem::path& folder, const std::vector<std::string>& lines) {
    std::string text;
    for (const std::string& line : lines) {
        text += line + "\n";
    }
    return WriteFileAtomically(folder.wstring(), L"reports.jsonl", text);
}

}  // namespace

bool IsQueueableLine(const std::string& line) {
    bool lengthOk = !line.empty() && line.size() <= kMaxQueueLineBytes;
    return lengthOk && line.find_first_of("\r\n") == std::string::npos;
}

QueueAppendResult AppendReport(const std::wstring& dataDir, const std::string& line) {
    QueueAppendResult answer;
    if (!IsQueueableLine(line)) {
        answer.outcome = QueueAppend::Refused;
        return answer;
    }
    if (!EnsureSubfolder(dataDir, L"queue").ok()) {
        return answer;
    }
    std::filesystem::path folder = QueueFolder(dataDir);
    Reading<QueueState> loaded = LoadQueue(folder / L"reports.jsonl");
    if (!loaded.ok()) {
        return answer;
    }
    QueueState state = std::move(loaded.value);
    if (state.oversized && !MoveBrokenFile(folder).ok()) {
        return answer;
    }
    state.lines.push_back(line);
    state.repairs.droppedOldest = TrimToCap(state.lines);
    if (!WriteQueue(folder, state.lines).ok()) {
        return answer;
    }
    answer.outcome = QueueAppend::Added;
    answer.repairs = state.repairs;
    return answer;
}

Reading<QueueBatch> PeekReports(const std::wstring& dataDir, int maxLines) {
    using Result = Reading<QueueBatch>;
    Reading<QueueState> loaded = LoadQueue(QueueFolder(dataDir) / L"reports.jsonl");
    if (!loaded.ok()) {
        return Result::Failure(loaded.status, loaded.error);
    }
    std::size_t wanted = static_cast<std::size_t>(std::clamp(maxLines, 1, kMaxBatchLines));
    std::size_t count = std::min(wanted, loaded.value.lines.size());
    QueueBatch batch;
    batch.lines.assign(loaded.value.lines.begin(), loaded.value.lines.begin() + static_cast<std::ptrdiff_t>(count));
    batch.repairs = loaded.value.repairs;
    return Result::Success(std::move(batch));
}

Reading<int> CountReports(const std::wstring& dataDir) {
    using Result = Reading<int>;
    Reading<QueueState> loaded = LoadQueue(QueueFolder(dataDir) / L"reports.jsonl");
    if (!loaded.ok()) {
        return Result::Failure(loaded.status, loaded.error);
    }
    return Result::Success(static_cast<int>(loaded.value.lines.size()));
}

Reading<bool> RemoveFirstReports(const std::wstring& dataDir, int count) {
    using Result = Reading<bool>;
    std::filesystem::path folder = QueueFolder(dataDir);
    Reading<QueueState> loaded = LoadQueue(folder / L"reports.jsonl");
    if (!loaded.ok()) {
        return Result::Failure(loaded.status, loaded.error);
    }
    QueueState state = std::move(loaded.value);
    if (!state.exists || count <= 0) {
        return Result::Success(true);
    }
    if (state.oversized) {
        return MoveBrokenFile(folder);
    }
    std::size_t removed = std::min(static_cast<std::size_t>(count), state.lines.size());
    state.lines.erase(state.lines.begin(), state.lines.begin() + static_cast<std::ptrdiff_t>(removed));
    return WriteQueue(folder, state.lines);
}

}  // namespace ews
