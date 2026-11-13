// report_json.cpp: the report JSON builder (see ews/report_json.h and SPEC 5.4).
// Used by ews-service. The text is written by hand with a fixed key order and no spaces.
#include "ews/report_json.h"

#include <algorithm>
#include <cmath>

#include <windows.h>

#include "ews/json_text.h"

namespace ews {

namespace {

constexpr std::size_t kMaxDeviceTypeLength = 40;

/// @return True for a to z.
bool IsLower(char c) { return c >= 'a' && c <= 'z'; }

/// @return True for a to z, 0 to 9 and the underscore.
bool IsDeviceTypeChar(char c) { return IsLower(c) || (c >= '0' && c <= '9') || c == '_'; }

/// @return True when the text matches ^[a-z][a-z0-9_]*$ and has 1 to 40 characters.
bool IsValidDeviceType(const std::string& text) {
    if (text.empty() || text.size() > kMaxDeviceTypeLength || !IsLower(text[0])) {
        return false;
    }
    return std::all_of(text.begin(), text.end(), IsDeviceTypeChar);
}

/// @return True when the numbers of the report are in range.
bool NumbersInRange(const ReportFields& fields) {
    if (fields.intervalSeconds < 1 || fields.idleSeconds < 0) {
        return false;
    }
    return fields.state >= 0 && fields.state <= 2;
}

/// @return A `"key":value` piece with the value as ready text.
std::string Member(const char* key, const std::string& valueText) { return std::string("\"") + key + "\":" + valueText; }

/// @return The text of a boolean.
const char* BoolText(bool value) { return value ? "true" : "false"; }

/// Appends `,"key":value` for a whole number that is known.
void AppendOptionalWhole(std::string& out, const char* key, const std::optional<std::int64_t>& value) {
    if (!value.has_value()) {
        return;
    }
    out += "," + Member(key, std::to_string(*value));
}

/// Appends `,"cpu_percent":x.y` when the percent is a finite number. The value is limited to 0 to 100;
/// adding 0.0 turns a negative zero into a plain zero.
void AppendCpuPercent(std::string& out, const std::optional<double>& percent) {
    if (!percent.has_value() || !std::isfinite(*percent)) {
        return;
    }
    double limited = std::clamp(*percent, 0.0, 100.0) + 0.0;
    out += "," + Member("cpu_percent", FormatFixed(limited, 1));
}

}  // namespace

Reading<std::string> BuildReportJson(const ReportFields& fields) {
    if (!IsValidDeviceType(fields.deviceType) || !NumbersInRange(fields)) {
        return Reading<std::string>::Failure(Status::Error, ERROR_INVALID_DATA);
    }
    std::string out = "{" + Member("device_id", JsonQuote(fields.deviceId));
    out += "," + Member("timestamp", JsonQuote(FormatUtcTime(fields.timestampUnix)));
    out += "," + Member("interval_seconds", std::to_string(fields.intervalSeconds));
    out += "," + Member("idle_seconds", std::to_string(fields.idleSeconds));
    out += "," + Member("state", std::to_string(fields.state));
    AppendCpuPercent(out, fields.cpuPercent);
    out += "," + Member("screen_locked", BoolText(fields.screenLocked));
    out += "," + Member("on_battery", BoolText(fields.onBattery));
    out += "," + Member("os", "\"windows\"");
    out += "," + Member("device_type", JsonQuote(fields.deviceType));
    AppendOptionalWhole(out, "energy_uj", fields.energyUj);
    AppendOptionalWhole(out, "battery_energy_uj", fields.batteryEnergyUj);
    AppendOptionalWhole(out, "device_energy_uj", fields.deviceEnergyUj);
    out += "}";
    return Reading<std::string>::Success(std::move(out));
}

}  // namespace ews
