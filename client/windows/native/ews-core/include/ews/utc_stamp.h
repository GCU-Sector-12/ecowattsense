// utc_stamp.h: formats a UTC time as text for file names, log lines and history rows.
// Used by the history file, the service log and the queue helpers of ews-service.
// The time is always a parameter, so tests can set it.
#pragma once

#include <chrono>
#include <string>

namespace ews {

/// A point in time; the callers pass UTC from system_clock.
using UtcTime = std::chrono::system_clock::time_point;

/// Writes the UTC date of a time as `yyyy-MM-dd`.
/// @param time The time to format.
/// @return For example `2026-11-14`.
std::string FormatUtcDate(UtcTime time);

/// Writes a time as `yyyy-MM-ddTHH:mm:ssZ` (UTC, whole seconds, the fraction is cut off).
/// @param time The time to format.
/// @return For example `2026-11-14T10:05:00Z`.
std::string FormatUtcTimestamp(UtcTime time);

}  // namespace ews
