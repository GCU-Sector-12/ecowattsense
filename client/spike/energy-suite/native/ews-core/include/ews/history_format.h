// history_format.h: the text of the 24 h history (SPEC section 7): the header line and one row per minute.
// Used by ews-service when it appends to <data>\history\<date>.csv, and by tests. No file is touched here.
// Neither the header nor a row holds a line break; the caller adds "\n".
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "ews/energy.h"

namespace ews {

/// The first line of every history file.
constexpr const char* kHistoryHeader = "v1,ts_utc,seconds,on_battery,state,idle_s,cpu_pct,device_w,measured_w,battery_w,level";

/// One minute of history. An empty optional is an unknown value and becomes an empty field.
struct HistoryRow {
    std::int64_t timestampUnix = 0;        ///< End of the minute, Unix seconds.
    std::int64_t seconds = 0;              ///< Length of the minute.
    bool onBattery = false;
    std::optional<int> state;              ///< 2, 1 or 0.
    std::optional<std::int64_t> idleSeconds;
    std::optional<double> cpuPct;          ///< One decimal.
    std::optional<double> deviceW;         ///< Three decimals.
    std::optional<double> measuredW;       ///< Three decimals.
    std::optional<double> batteryW;        ///< Three decimals; negative when discharging.
    MeasureLevel level = MeasureLevel::Unknown;
};

/// Formats one row: `v1,<ts>,<seconds>,<0|1>,<state>,<idle>,<cpu>,<device_w>,<measured_w>,<battery_w>,<level>`.
/// The time is as in the report (2026-11-14T10:05:00Z). on_battery is 1 or 0. A non-finite number is an empty field.
/// @param row The values.
/// @return The line without a line break.
std::string FormatHistoryRow(const HistoryRow& row);

}  // namespace ews
