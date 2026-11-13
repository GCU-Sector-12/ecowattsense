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

}  // namespace

const char* SendResultWord(SendResult result) {
    switch (result) {
        case SendResult::Ok: return "ok";
        case SendResult::Disabled: return "disabled";
        case SendResult::Unauthorised: return "unauthorised";
        case SendResult::Rejected: return "rejected";
        case SendResult::ServerError: return "server_error";
        case SendResult::Network: return "network";
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
    out += ",\"queued\":" + std::to_string(fields.queued) + "}\n";
    return out;
}

}  // namespace ews
