// diag_line.cpp: builds the lines of the diagnostics file (see ews/diag_line.h and SPEC 7a).
// Used by ServiceCore (service_diag.cpp) and the tests. Pure text work: values in, one line out.
// Rules kept here: `t` and `ts` come first; an unknown (empty) value is left out and never becomes 0; every text is
// escaped with JsonQuote, so a channel name cannot break a line; no line holds a line feed.
#include "ews/diag_line.h"

#include <cmath>

#include "ews/json_text.h"

namespace ews {

namespace {

constexpr int kWattDecimals = 3;
constexpr int kPercentDecimals = 1;

/// Builds the text of one JSON object, field by field. An unknown value is skipped.
class Obj {
public:
    /// Adds a field with a ready JSON value; an empty value is skipped.
    Obj& Json(const std::string& key, const std::string& json) {
        if (json.empty()) {
            return *this;
        }
        text_ += (first_ ? "" : ",") + JsonQuote(key) + ":" + json;
        first_ = false;
        return *this;
    }

    /// Adds a text field; an empty text is skipped.
    Obj& Text(const std::string& key, const std::string& value) { return Json(key, value.empty() ? std::string() : JsonQuote(value)); }

    /// Adds a bool field.
    Obj& Bool(const std::string& key, bool value) { return Json(key, value ? "true" : "false"); }

    /// Adds a signed whole number; nothing when it is unknown.
    Obj& Int(const std::string& key, const std::optional<std::int64_t>& value) {
        return Json(key, value.has_value() ? std::to_string(*value) : std::string());
    }

    /// Adds an unsigned whole number; nothing when it is unknown.
    Obj& Uint(const std::string& key, const std::optional<std::uint64_t>& value) {
        return Json(key, value.has_value() ? std::to_string(*value) : std::string());
    }

    /// Adds a number with fixed decimals; nothing when it is unknown or not finite.
    Obj& Number(const std::string& key, const std::optional<double>& value, int decimals);

    /// @return The object text with its braces.
    std::string Close() const { return "{" + text_ + "}"; }

private:
    std::string text_;
    bool first_ = true;
};

/// @return The number as fixed decimals, without a minus sign in front of a zero (-0.000 becomes 0.000).
std::string NumberText(double value, int decimals) {
    std::string text = FormatFixed(value, decimals);
    bool negative = !text.empty() && text[0] == '-';
    bool zero = negative && text.find_first_not_of("0.", 1) == std::string::npos;
    return zero ? text.substr(1) : text;
}

Obj& Obj::Number(const std::string& key, const std::optional<double>& value, int decimals) {
    bool usable = value.has_value() && std::isfinite(*value);
    return Json(key, usable ? NumberText(*value, decimals) : std::string());
}

/// @return The start of a line: the kind first, then the time.
Obj Start(const char* kind, std::int64_t unixSeconds) {
    Obj line;
    line.Text("t", kind).Text("ts", FormatUtcTime(unixSeconds));
    return line;
}

/// @return The parts as a JSON array of ready values.
std::string JoinArray(const std::vector<std::string>& parts) {
    std::string text = "[";
    for (std::size_t index = 0; index < parts.size(); ++index) {
        text += (index == 0 ? "" : ",") + parts[index];
    }
    return text + "]";
}

/// @return One EMI channel as an object. A failed read has no `raw` and no `uj`.
std::string EmiJson(const DiagEmi& channel) {
    Obj object;
    object.Text("name", channel.name).Text("kind", channel.kind).Bool("ok", channel.ok);
    object.Uint("raw", channel.ok ? channel.raw : std::nullopt);
    object.Uint("uj", channel.ok ? channel.uj : std::nullopt);
    return object.Close();
}

/// Adds the capacity and rate fields of a battery. A relative unit has no mWh and no mW values.
void AddBatteryValues(Obj& object, const DiagBattery& battery) {
    bool mwh = battery.unitMwh;
    object.Uint("remaining_mwh", mwh ? battery.remaining : std::nullopt);
    object.Uint("full_mwh", mwh ? battery.full : std::nullopt);
    object.Uint("design_mwh", mwh ? battery.design : std::nullopt);
    object.Uint("voltage_mv", battery.voltage);
    object.Int("rate_mw", mwh ? battery.rate : std::nullopt);
}

/// @return One battery as an object. A failed read has only `ok`.
std::string BatteryJson(const DiagBattery& battery) {
    Obj object;
    object.Bool("ok", battery.ok);
    if (!battery.ok) {
        return object.Close();
    }
    object.Text("unit", battery.unitMwh ? "mWh" : "relative");
    AddBatteryValues(object, battery);
    object.Bool("online", battery.online).Bool("charging", battery.charging).Bool("discharging", battery.discharging);
    return object.Close();
}

/// @return The processor object, or an empty text when the percent is unknown.
std::string CpuJson(const std::optional<double>& percent) {
    Obj object;
    object.Number("pct", percent, kPercentDecimals);
    return percent.has_value() && std::isfinite(*percent) ? object.Close() : std::string();
}

/// @return The display object.
std::string DisplayJson(const DiagDisplay& display) {
    Obj object;
    object.Bool("on", display.on).Int("brightness_pct", display.brightnessPct).Text("brightness_from", display.from);
    return object.Close();
}

/// @return The session object.
std::string SessionJson(const DiagSession& session) {
    Obj object;
    object.Bool("logged_on", session.loggedOn).Bool("locked", session.locked);
    object.Int("idle_s", session.idleS).Int("idle_msg_age_s", session.idleMsgAgeS).Int("state", session.state);
    return object.Close();
}

/// @return The calculation object: the level, the model flag and each number the minute was calculated with.
std::string CalcJson(const DiagCalc& calc) {
    Obj object;
    object.Text("scan_level", calc.scanLevel).Text("level", calc.level).Bool("model", calc.model).Int("measured_uj", calc.measuredUj);
    object.Number("base_w", calc.baseW, kWattDecimals).Number("rest_w", calc.restW, kWattDecimals);
    object.Number("cpu_term_w", calc.cpuTermW, kWattDecimals).Number("screen_w", calc.screenW, kWattDecimals);
    object.Number("device_w", calc.deviceW, kWattDecimals).Number("battery_w", calc.batteryW, kWattDecimals);
    object.Int("energy_uj", calc.energyUj);
    return object.Close();
}

/// @return A list of texts as a JSON array of strings.
std::string TextArray(const std::vector<std::string>& words) {
    std::vector<std::string> parts;
    for (const std::string& word : words) {
        parts.push_back(JsonQuote(word));
    }
    return JoinArray(parts);
}

/// @return True when the text has a carriage return or a line feed.
bool HasLineBreak(const std::string& text) { return text.find_first_of("\r\n") != std::string::npos; }

}  // namespace

DiagField FieldBool(const std::string& key, bool value) { return DiagField{key, value ? "true" : "false"}; }

DiagField FieldInt(const std::string& key, long long value) { return DiagField{key, std::to_string(value)}; }

DiagField FieldMaybeInt(const std::string& key, const std::optional<long long>& value) {
    return DiagField{key, value.has_value() ? std::to_string(*value) : std::string()};
}

DiagField FieldWord(const std::string& key, const std::string& word) { return DiagField{key, JsonQuote(word)}; }

std::string BuildTickLine(const DiagTick& tick) {
    Obj line = Start("tick", tick.unixSeconds);
    line.Int("seconds", tick.seconds);
    line.Json("power", Obj().Bool("on_battery", tick.onBattery).Close());
    std::vector<std::string> channels;
    for (const DiagEmi& channel : tick.emi) {
        channels.push_back(EmiJson(channel));
    }
    std::vector<std::string> batteries;
    for (const DiagBattery& battery : tick.battery) {
        batteries.push_back(BatteryJson(battery));
    }
    line.Json("emi", JoinArray(channels)).Json("battery", JoinArray(batteries)).Json("cpu", CpuJson(tick.cpuPct));
    line.Json("display", DisplayJson(tick.display)).Json("session", SessionJson(tick.session));
    line.Json("calc", CalcJson(tick.calc)).Json("flags", TextArray(tick.flags));
    return line.Close();
}

std::string BuildReportLine(std::int64_t unixSeconds, int queue, const std::string& reportJson) {
    if (reportJson.empty() || HasLineBreak(reportJson)) {
        return std::string();
    }
    Obj line = Start("report", unixSeconds);
    line.Int("queue", queue).Json("report", reportJson);
    return line.Close();
}

std::string BuildSendLine(std::int64_t unixSeconds, const DiagSend& send) {
    Obj line = Start("send", unixSeconds);
    line.Text("trigger", send.trigger).Int("reports", send.reports).Text("result", send.result).Int("http", send.http);
    line.Int("created", send.created).Int("duplicate", send.duplicate).Int("rejected", send.rejected).Int("queue", send.queue);
    return line.Close();
}

std::string BuildEventLine(std::int64_t unixSeconds, const std::string& what, const std::vector<DiagField>& fields) {
    Obj line = Start("event", unixSeconds);
    line.Text("what", what);
    for (const DiagField& field : fields) {
        line.Json(field.key, field.json);
    }
    return line.Close();
}

bool FitsDiagLine(const std::string& line) { return line.size() + 1 <= kMaxDiagLineBytes; }

}  // namespace ews
