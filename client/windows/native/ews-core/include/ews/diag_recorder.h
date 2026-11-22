// diag_recorder.h: the rules around the diagnostics file (SPEC 7a): file names, the 7 day rule, the 16 KB and 20 MB
// rules, and the counting of failed writes. The file itself is written by an IDiagWriter (diag_file.h in the service,
// a fake in the tests). No Windows call is made here.
// A failed write never stops measuring: the recorder counts it and tells its caller when the log needs a word.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "ews/utc_stamp.h"

namespace ews {

inline constexpr std::uint64_t kMaxDiagFileBytes = 20ull * 1024 * 1024;  ///< The most bytes of one day's file.
inline constexpr int kDiagKeepDays = 7;                                  ///< Files older than this many days are deleted.
inline constexpr std::int64_t kDiagNoteSeconds = 3600;                   ///< The log gets diag_write_failed at most once an hour.

/// @param time A time.
/// @return `diag-yyyy-MM-dd.jsonl` for the UTC date of the time.
std::wstring DiagFileName(UtcTime time);

/// @param name A file name without folder.
/// @return True when the name is exactly `diag-yyyy-MM-dd.jsonl` with a real date.
bool IsDiagFileName(const std::wstring& name);

/// @param name A file name without folder.
/// @param now The current time.
/// @return True when the name is a diagnostics file name and its date is more than kDiagKeepDays days before the UTC date of `now`.
bool DiagFileIsOld(const std::wstring& name, UtcTime now);

/// The result of one append.
enum class DiagWriteResult {
    Written,  ///< The line is in the file and flushed.
    Full,     ///< The file of that day has reached kMaxDiagFileBytes; nothing was written.
    Failed    ///< The line could not be written (open, folder, disk, sharing).
};

/// Where the lines go. The service uses a file; tests use a fake.
class IDiagWriter {
public:
    virtual ~IDiagWriter() = default;
    /// Appends one line (the writer adds the line feed) to the file of the UTC date of `time`.
    /// @param time The time of the line.
    /// @param line The line without a line feed, at most kMaxDiagLineBytes.
    /// @return Written, Full or Failed. Never throws.
    virtual DiagWriteResult Append(UtcTime time, const std::string& line) = 0;
    /// Deletes the diagnostics files that DiagFileIsOld names. A failure is ignored.
    /// @param now The current time.
    virtual void RemoveOld(UtcTime now) = 0;
};

/// What the caller must put into the service log after a Record.
enum class DiagNote { None, Full, WriteFailed };

/// The outcome of Record.
struct DiagOutcome {
    DiagNote note = DiagNote::None;
    int failures = 0;  ///< For WriteFailed: how many lines have failed since the start.
};

/// Applies the rules and counts. One instance for the whole run of the service; one thread.
class DiagRecorder {
public:
    /// @param writer Where the lines go; nothing is written (and nothing counted) when it is null. It must outlive the recorder.
    explicit DiagRecorder(IDiagWriter* writer);

    /// Writes one line unless a rule stops it.
    /// A line that is empty or longer than kMaxDiagLineBytes is not written and is counted as a failure.
    /// After `Full` nothing more is written for that UTC date. A failed write is counted; the first one and then one an hour is reported.
    /// @param time The time of the line.
    /// @param line The line without a line feed.
    /// @return What the log needs, if anything.
    DiagOutcome Record(UtcTime time, const std::string& line);

    /// Deletes the files older than 7 days (at the start and at the change of day).
    /// @param now The current time.
    void Clean(UtcTime now);

    /// @return True when there is a writer. Callers skip the work of building a line when it is false.
    bool Enabled() const;

    /// @return How many lines failed or were refused since the start.
    int Failures() const;

private:
    DiagOutcome CountFailure(UtcTime time);

    IDiagWriter* writer_;
    int failures_ = 0;
    bool noted_ = false;
    UtcTime lastNote_{};
    std::string fullDay_;
};

}  // namespace ews
