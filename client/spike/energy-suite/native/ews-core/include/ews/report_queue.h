// report_queue.h: the queue file `<data>\queue\reports.jsonl` (SPEC 5.5): one report per line, oldest first.
// Used by ews-service: it appends a report at each report tick and removes the lines the server took.
// Every change rewrites the file atomically, so a crash leaves the old file. Counts are returned so the caller can log them.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "ews/reading.h"

namespace ews {

inline constexpr int kMaxQueueLines = 2016;                           ///< 7 days of 5 minute reports.
inline constexpr std::size_t kMaxQueueLineBytes = 4 * 1024;           ///< The longest accepted line, without its line feed.
inline constexpr std::size_t kMaxQueueFileBytes = 16u * 1024 * 1024;  ///< A larger file counts as broken.
inline constexpr int kMaxBatchLines = 100;                            ///< The most lines of one send.

/// Counts of the damage the queue code found and repaired. All are zero for a healthy file.
struct QueueRepairs {
    int skippedLines = 0;   ///< Lines left out: broken last line, empty line, too long line, line with a carriage return.
    int brokenFiles = 0;    ///< Files over 16 MB that were renamed to `reports.broken.jsonl`.
    int droppedOldest = 0;  ///< Oldest lines dropped because the queue was full (append only).
};

/// What AppendReport did.
enum class QueueAppend {
    Added,    ///< The line is at the end of the queue.
    Refused,  ///< The line is empty, over 4 KB or has a line break; nothing was changed.
    Failed    ///< The queue could not be read or written; nothing was changed.
};

/// The result of AppendReport.
struct QueueAppendResult {
    QueueAppend outcome = QueueAppend::Failed;
    QueueRepairs repairs;
};

/// A batch read from the front of the queue.
struct QueueBatch {
    std::vector<std::string> lines;  ///< Up to the asked number of valid lines, oldest first.
    QueueRepairs repairs;
};

/// Checks that a line can be queued.
/// @param line The text.
/// @return True for 1 to 4096 bytes with no line feed and no carriage return.
bool IsQueueableLine(const std::string& line);

/// Adds one line at the end. A broken last line is cut first. When the queue then holds more than 2016 lines
/// the oldest are dropped and counted. A queue file over 16 MB is renamed to `reports.broken.jsonl` and a new queue starts.
/// @param dataDir The data folder; the `queue` sub folder is created when missing.
/// @param line One report without a line break; 1 to 4096 bytes.
/// @return The outcome and the counts.
QueueAppendResult AppendReport(const std::wstring& dataDir, const std::string& line);

/// Reads the first lines of the queue without changing the file. A missing file is an empty queue.
/// A file over 16 MB gives an empty batch with `brokenFiles` 1; the file itself is renamed by the next AppendReport
/// or RemoveFirstReports.
/// @param dataDir The data folder.
/// @param maxLines The most lines to return (1 to 100; other values are clamped into that range).
/// @return Ok with the batch, or the failure when the file cannot be read.
Reading<QueueBatch> PeekReports(const std::wstring& dataDir, int maxLines);

/// Counts the valid lines of the queue without changing the file. A missing file is an empty queue.
/// @param dataDir The data folder.
/// @return Ok with the number of valid lines (a file over 16 MB counts as 0), or the failure when the file cannot be read.
Reading<int> CountReports(const std::wstring& dataDir);

/// Removes the first `count` valid lines (and the skipped lines in front of them) by an atomic rewrite.
/// @param dataDir The data folder.
/// @param count How many lines to remove; the same lines PeekReports gave. More than there are empties the queue.
/// @return Ok with true, or the failure. On failure the queue is unchanged.
Reading<bool> RemoveFirstReports(const std::wstring& dataDir, int count);

}  // namespace ews
