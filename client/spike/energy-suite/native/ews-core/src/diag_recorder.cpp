// diag_recorder.cpp: the rules around the diagnostics file (see ews/diag_recorder.h and SPEC 7a).
// Used by ServiceCore through DiagRecorder and by the file writer (diag_file.cpp) for the names. Pure rules: no file is touched.
#include "ews/diag_recorder.h"

#include <chrono>

#include "ews/diag_line.h"
#include "ews/json_text.h"

namespace ews {

namespace {

constexpr std::wstring_view kPrefix = L"diag-";
constexpr std::wstring_view kSuffix = L".jsonl";
constexpr std::size_t kDateLength = 10;  // yyyy-MM-dd
constexpr std::int64_t kSecondsPerDay = 86400;

/// @return True when every character is plain ASCII, so narrowing to char cannot turn a letter into a digit.
bool IsAscii(std::wstring_view text) {
    for (wchar_t c : text) {
        if (c >= 128) {
            return false;
        }
    }
    return true;
}

/// @param name A file name.
/// @return The date text `yyyy-MM-dd` of a name with the right shape, or an empty text.
std::string DateOfName(const std::wstring& name) {
    std::wstring_view view(name);
    bool shape = view.size() == kPrefix.size() + kDateLength + kSuffix.size() && view.substr(0, kPrefix.size()) == kPrefix &&
                 view.substr(view.size() - kSuffix.size()) == kSuffix;
    std::wstring_view date = shape ? view.substr(kPrefix.size(), kDateLength) : std::wstring_view();
    if (date.empty() || !IsAscii(date)) {
        return std::string();
    }
    return std::string(date.begin(), date.end());
}

/// @param date `yyyy-MM-dd`.
/// @param unixSeconds Receives the start of that day.
/// @return True when the text is a real date.
bool DayStart(const std::string& date, std::int64_t& unixSeconds) { return ParseUtcTime(date + "T00:00:00Z", unixSeconds); }

/// @return The start (00:00:00 UTC) of the UTC day of a time, in Unix seconds.
std::int64_t StartOfDay(UtcTime time) {
    std::int64_t seconds = std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
    std::int64_t days = seconds / kSecondsPerDay - (seconds % kSecondsPerDay < 0 ? 1 : 0);
    return days * kSecondsPerDay;
}

}  // namespace

std::wstring DiagFileName(UtcTime time) {
    std::string date = FormatUtcDate(time);
    return std::wstring(kPrefix) + std::wstring(date.begin(), date.end()) + std::wstring(kSuffix);
}

bool IsDiagFileName(const std::wstring& name) {
    std::int64_t ignored = 0;
    std::string date = DateOfName(name);
    return !date.empty() && DayStart(date, ignored);
}

bool DiagFileIsOld(const std::wstring& name, UtcTime now) {
    std::int64_t fileDay = 0;
    std::string date = DateOfName(name);
    if (date.empty() || !DayStart(date, fileDay)) {
        return false;
    }
    return fileDay < StartOfDay(now) - kDiagKeepDays * kSecondsPerDay;
}

DiagRecorder::DiagRecorder(IDiagWriter* writer) : writer_(writer) {}

DiagOutcome DiagRecorder::Record(UtcTime time, const std::string& line) {
    if (writer_ == nullptr) {
        return DiagOutcome{};
    }
    bool usable = !line.empty() && FitsDiagLine(line);
    if (!usable) {
        return CountFailure(time);
    }
    std::string day = FormatUtcDate(time);
    if (day == fullDay_) {
        return DiagOutcome{};
    }
    DiagWriteResult result = writer_->Append(time, line);
    if (result == DiagWriteResult::Written) {
        return DiagOutcome{};
    }
    if (result == DiagWriteResult::Full) {
        fullDay_ = day;
        return DiagOutcome{DiagNote::Full, 0};
    }
    return CountFailure(time);
}

DiagOutcome DiagRecorder::CountFailure(UtcTime time) {
    ++failures_;
    std::chrono::seconds since = std::chrono::duration_cast<std::chrono::seconds>(time - lastNote_);
    bool due = !noted_ || since.count() >= kDiagNoteSeconds || since.count() < 0;
    if (!due) {
        return DiagOutcome{};
    }
    noted_ = true;
    lastNote_ = time;
    return DiagOutcome{DiagNote::WriteFailed, failures_};
}

void DiagRecorder::Clean(UtcTime now) {
    if (writer_ != nullptr) {
        writer_->RemoveOld(now);
    }
}

int DiagRecorder::Failures() const { return failures_; }

}  // namespace ews
