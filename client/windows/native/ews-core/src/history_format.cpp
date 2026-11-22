// history_format.cpp: the history row writer (see ews/history_format.h and SPEC section 7).
// Used by ews-service. An unknown value is an empty field. Numbers use a dot and no locale.
#include "ews/history_format.h"

#include <cmath>

#include "ews/json_text.h"

namespace ews {

namespace {

/// @return The number with fixed decimals, or an empty text when it is unknown or not finite.
std::string DecimalField(const std::optional<double>& value, int decimals) {
    if (!value.has_value() || !std::isfinite(*value)) {
        return std::string();
    }
    return FormatFixed(*value, decimals);
}

/// @return The whole number as text, or an empty text when it is unknown.
std::string WholeField(const std::optional<std::int64_t>& value) {
    if (!value.has_value()) {
        return std::string();
    }
    return std::to_string(*value);
}

/// @return The state as text, or an empty text when it is unknown.
std::string StateField(const std::optional<int>& state) {
    if (!state.has_value()) {
        return std::string();
    }
    return std::to_string(*state);
}

}  // namespace

std::string FormatHistoryRow(const HistoryRow& row) {
    std::string line = "v1";
    line += "," + FormatUtcTime(row.timestampUnix);
    line += "," + std::to_string(row.seconds);
    line += row.onBattery ? ",1" : ",0";
    line += "," + StateField(row.state);
    line += "," + WholeField(row.idleSeconds);
    line += "," + DecimalField(row.cpuPct, 1);
    line += "," + DecimalField(row.deviceW, 3);
    line += "," + DecimalField(row.measuredW, 3);
    line += "," + DecimalField(row.batteryW, 3);
    line += std::string(",") + MeasureLevelWord(row.level);
    return line;
}

}  // namespace ews
