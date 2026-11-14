// service_log.h: the service log `<data>\logs\service.log` (SPEC 5.8).
// Used by ews-service. A line holds a UTC time, an event word and up to three whole numbers, nothing else:
// no URL, no key, no file content, no server text. No free text can be passed in.
// At 1 MB the file is renamed to `service.previous.log` (an older one is replaced) and a new file starts.
#pragma once

#include <initializer_list>
#include <string>

#include "ews/reading.h"
#include "ews/utc_stamp.h"

namespace ews {

inline constexpr unsigned long long kMaxLogBytes = 1024 * 1024;  ///< The size at which the log rolls over.

/// The fixed events of the log.
enum class LogEvent {
    ServiceStart,
    ServiceStop,
    LevelChange,       ///< numbers: old level, new level.
    SendOk,            ///< numbers: lines sent, reports rejected.
    SendUnauthorised,
    SendRejected,      ///< numbers: lines sent.
    SendServerError,
    SendNetwork,
    SendDisabled,
    ConfigBadValues,   ///< numbers: how many keys had a wrong value.
    QueueDropped,      ///< numbers: how many oldest reports were dropped.
    QueueSkipped,      ///< numbers: how many damaged lines were skipped.
    QueueBrokenFile,
    QueueWriteFailed,
    HistoryWriteFailed,
    HistoryCleaned,    ///< numbers: how many old files were deleted.
    CountedError,      ///< numbers: an error code chosen by the caller.
    StartFailed,       ///< numbers: a reason code (1 data folder missing, 2 lock error, 3 identity failed).
    LockHeld,          ///< Another program holds Client\instance.lock; the service stops.
    ModelInvalid,      ///< model.json exists but is not a valid model.
    ReportSkipped,     ///< numbers: reports not built because the processor percent was unknown.
    PipeDropped,       ///< numbers: pipe messages dropped since the last report (bad or from a session that is not active).
    ScanWriteFailed,   ///< The scan result could not be written.
    StatusWriteFailed, ///< status.json could not be written.
    GapDetected,       ///< A tick came much later than expected (long sleep); the interval was closed at the last read.
    BatteryJump,       ///< numbers: how many impossible battery changes were found since the last report.
    BatteryReplaced,   ///< numbers: how many impossible battery changes were replaced by a later possible one since the last report.
    BatterySplit       ///< numbers: how many held minutes shared one battery fall in a history split (SPEC 7).
};

/// @param event An event.
/// @return Its fixed lower case word, for example `send_ok`.
const char* LogEventWord(LogEvent event);

/// Writes one log line: `<yyyy-MM-ddTHH:mm:ssZ> <word> [n1 [n2 [n3]]]` and a line feed. Pure; used by AppendLog.
/// @param time The time of the event.
/// @param event The event.
/// @param numbers Up to three whole numbers; any further ones are ignored.
/// @return The line.
std::string FormatLogLine(UtcTime time, LogEvent event, std::initializer_list<long long> numbers);

/// Appends one line to `<data>\logs\service.log`, rolling the file over first when it is at 1 MB.
/// The line goes out in one write, and the file is opened so that another process may append and rename it too.
/// @param dataDir The data folder; the `logs` sub folder is created when missing.
/// @param time The time of the event.
/// @param event The event.
/// @param numbers Up to three whole numbers.
/// @return Ok with true, or the failure with the Windows error code. A failed roll-over is not an error: the line is
///         then appended to the current file.
Reading<bool> AppendLog(const std::wstring& dataDir, UtcTime time, LogEvent event, std::initializer_list<long long> numbers = {});

}  // namespace ews
