// history_file.h: the daily history file `<data>\history\<yyyy-MM-dd>.csv` (SPEC 7).
// Used by ews-service: one row per minute is appended, and old files are deleted at the change of day.
// The file is opened for each row (so a change of day never leaves a handle on an old file) with shared read.
#pragma once

#include <string>

#include "ews/reading.h"
#include "ews/utc_stamp.h"

namespace ews {

/// The number of whole days of history that are kept besides today.
inline constexpr int kHistoryKeepDays = 2;

/// Writes the file name of a UTC day.
/// @param time The time; its UTC date is used.
/// @return For example `2026-11-14.csv`.
std::wstring HistoryFileName(UtcTime time);

/// Checks whether a name is exactly `yyyy-MM-dd.csv` with a real calendar date.
/// @param name A file name without folder.
/// @return True for a history file name.
bool IsHistoryFileName(const std::wstring& name);

/// Appends one row to the file of the UTC date of `time`. A new (or empty) file first gets the header line.
/// The header and the row go out in one write, so two processes never split a row.
/// @param dataDir The data folder; the `history` sub folder is created when missing.
/// @param time The time of the row; its UTC date picks the file.
/// @param header The header line, used only for a new file. No line break.
/// @param row The row. No line break, not empty.
/// @return Ok with true, or the failure with the Windows error code. A line break in `header` or `row` is an Error.
Reading<bool> AppendHistoryRow(const std::wstring& dataDir, UtcTime time, const std::string& header, const std::string& row);

/// Deletes the files of `<data>\history` whose name is exactly a valid date and `.csv` and whose date is more than
/// 2 days before the UTC date of `now`. No other file is touched.
/// @param dataDir The data folder.
/// @param now The current time.
/// @return Ok with the number of deleted files (a missing folder gives Ok with 0), or the failure.
Reading<int> DeleteOldHistoryFiles(const std::wstring& dataDir, UtcTime now);

}  // namespace ews
