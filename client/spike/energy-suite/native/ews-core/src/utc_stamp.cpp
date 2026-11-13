// utc_stamp.cpp: formats UTC times as text (see ews/utc_stamp.h).
// Used by the history file and the service log.
#include "ews/utc_stamp.h"

#include <cstdio>

namespace ews {

namespace {

/// The calendar date and the time of day of a time, in whole seconds.
struct Broken {
    std::chrono::year_month_day date;
    std::chrono::hh_mm_ss<std::chrono::seconds> clock;
};

/// Splits a time into date and time of day. The fraction of a second is cut off (rounded down).
Broken Split(UtcTime time) {
    auto seconds = std::chrono::floor<std::chrono::seconds>(time);
    auto days = std::chrono::floor<std::chrono::days>(seconds);
    return Broken{std::chrono::year_month_day{days}, std::chrono::hh_mm_ss<std::chrono::seconds>{seconds - days}};
}

}  // namespace

std::string FormatUtcDate(UtcTime time) {
    Broken parts = Split(time);
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%04d-%02u-%02u", static_cast<int>(parts.date.year()),
                  static_cast<unsigned>(parts.date.month()), static_cast<unsigned>(parts.date.day()));
    return buffer;
}

std::string FormatUtcTimestamp(UtcTime time) {
    Broken parts = Split(time);
    char buffer[48];
    std::snprintf(buffer, sizeof buffer, "%04d-%02u-%02uT%02d:%02d:%02dZ", static_cast<int>(parts.date.year()),
                  static_cast<unsigned>(parts.date.month()), static_cast<unsigned>(parts.date.day()),
                  static_cast<int>(parts.clock.hours().count()), static_cast<int>(parts.clock.minutes().count()),
                  static_cast<int>(parts.clock.seconds().count()));
    return buffer;
}

}  // namespace ews
