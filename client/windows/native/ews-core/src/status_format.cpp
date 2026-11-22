// status_format.cpp: the status.json writer (see ews/status_format.h and SPEC 5.7).
// Used by ews-service. The text is built by hand; a field with an unknown value is left out.
#include "ews/status_format.h"

#include "ews/json_text.h"

namespace ews {

namespace {

/// Appends `,"key":"text"` when the text is known.
void AppendOptionalText(std::string& out, const char* key, const std::optional<std::string>& value) {
    if (!value.has_value()) {
        return;
    }
    out += std::string(",\"") + key + "\":" + JsonQuote(*value);
}

/// Appends `,"key":"time"` when the Unix time is known.
void AppendOptionalTime(std::string& out, const char* key, const std::optional<std::int64_t>& value) {
    if (!value.has_value()) {
        return;
    }
    out += std::string(",\"") + key + "\":" + JsonQuote(FormatUtcTime(*value));
}

/// Appends `,"server_device_name":"text"` or `,"server_device_name":null` when a PUT answer was seen.
void AppendServerName(std::string& out, const StatusFields& fields) {
    if (!fields.serverNameKnown) {
        return;
    }
    std::string value = fields.serverDeviceName.has_value() ? JsonQuote(*fields.serverDeviceName) : std::string("null");
    out += ",\"server_device_name\":" + value;
}

}  // namespace

const char* SendResultWord(LastSend result) {
    switch (result) {
        case LastSend::Ok: return "ok";
        case LastSend::Disabled: return "disabled";
        case LastSend::Unauthorised: return "unauthorised";
        case LastSend::Rejected: return "rejected";
        case LastSend::ServerError: return "server_error";
        case LastSend::Network: return "network";
        default: return "never";
    }
}

const char* PingResultWord(LastPing result) {
    switch (result) {
        case LastPing::Ok: return "ok";
        case LastPing::Unauthorised: return "unauthorised";
        case LastPing::RateLimited: return "rate_limited";
        case LastPing::ServerError: return "server_error";
        case LastPing::Network: return "network";
        case LastPing::Rejected: return "rejected";
        default: return "never";
    }
}

std::string BuildStatusJson(const StatusFields& fields) {
    std::string out = "{\"schema\":1,\"service_version\":" + JsonQuote(fields.serviceVersion);
    AppendOptionalText(out, "level", fields.level);
    AppendOptionalText(out, "model_generated_utc", fields.modelGeneratedUtc);
    AppendOptionalTime(out, "last_report_utc", fields.lastReportUnix);
    AppendOptionalTime(out, "last_send_utc", fields.lastSendUnix);
    out += std::string(",\"last_send_result\":\"") + SendResultWord(fields.lastSendResult) + "\"";
    out += ",\"queued\":" + std::to_string(fields.queued);
    out += ",\"server_url\":" + JsonQuote(fields.serverUrl);
    out += std::string(",\"server_key_set\":") + (fields.serverKeySet ? "true" : "false");
    out += std::string(",\"server_enabled\":") + (fields.serverEnabled ? "true" : "false");
    out += ",\"device_type\":" + JsonQuote(fields.deviceType);
    out += ",\"interval_seconds\":" + std::to_string(fields.intervalSeconds);
    out += ",\"idle_threshold_seconds\":" + std::to_string(fields.idleThresholdSeconds);
    out += ",\"unused_threshold_seconds\":" + std::to_string(fields.unusedThresholdSeconds);
    out += std::string(",\"settings_source\":\"") + (fields.settingsFromServer ? "server" : "local") + "\"";
    bool showVersion = fields.settingsFromServer && fields.settingsVersion.has_value();
    out += showVersion ? ",\"settings_version\":" + std::to_string(*fields.settingsVersion) : "";
    AppendOptionalTime(out, "last_settings_utc", fields.lastSettingsUnix);
    AppendOptionalTime(out, "last_ping_utc", fields.lastPingUnix);
    out += std::string(",\"last_ping_result\":\"") + PingResultWord(fields.lastPingResult) + "\"";
    AppendOptionalText(out, "device_name", fields.deviceName);
    AppendServerName(out, fields);
    out += fields.nameSync != NameSync::None ? std::string(",\"name_sync\":\"") + NameSyncWord(fields.nameSync) + "\"" : "";
    out += "}\n";
    return out;
}

}  // namespace ews
