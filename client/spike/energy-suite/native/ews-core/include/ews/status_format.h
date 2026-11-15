// status_format.h: the JSON text of status.json (SPEC 5.7).
// Used by ews-service after each report tick, and read by the tray client. The caller writes it to a temporary
// file and renames it. A field with an unknown value is left out, as in capabilities.json.
#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace ews {

/// The result of the last send attempt (the seven words of SPEC 5.7).
enum class LastSend { Ok, Disabled, Unauthorised, Rejected, ServerError, Network, Never };

/// @param result A send result.
/// @return "ok", "disabled", "unauthorised", "rejected", "server_error", "network" or "never".
const char* SendResultWord(LastSend result);

/// The result of the last ping (SPEC 5.7): the words of `last_ping_result`.
enum class LastPing { Ok, Unauthorised, RateLimited, ServerError, Network, Rejected, Never };

/// @param result A ping result.
/// @return "ok", "unauthorised", "rate_limited", "server_error", "network", "rejected" or "never".
const char* PingResultWord(LastPing result);

/// The values of status.json.
struct StatusFields {
    std::string serviceVersion;                   ///< For example "0.1.0".
    std::optional<std::string> level;             ///< "L1" to "L4"; empty when unknown.
    std::optional<std::string> modelGeneratedUtc; ///< generated_utc of the model; empty when there is no valid model.
    std::optional<std::int64_t> lastReportUnix;
    std::optional<std::int64_t> lastSendUnix;
    LastSend lastSendResult = LastSend::Never;
    std::uint32_t queued = 0;                     ///< Reports waiting in the queue.
    std::string serverUrl;                        ///< The URL in use; empty when none (missing or bad).
    bool serverKeySet = false;                    ///< True when key.bin was read and decrypted. The key itself is never written.
    bool serverEnabled = false;                   ///< `server_enabled` of service.ini.
    std::string deviceType;                       ///< `device_type` in use.
    int intervalSeconds = 0;                      ///< `interval_seconds` in use.
    int idleThresholdSeconds = 0;                 ///< `idle_threshold_seconds` in use.
    int unusedThresholdSeconds = 0;               ///< `unused_threshold_seconds` in use.
    bool settingsFromServer = false;              ///< True when the settings in force come from the server file.
    std::optional<std::int64_t> settingsVersion;  ///< `settings_version` in force; written only when the source is the server.
    std::optional<std::int64_t> lastSettingsUnix; ///< The time of the last good settings read.
    std::optional<std::int64_t> lastPingUnix;     ///< The time of the last ping attempt.
    LastPing lastPingResult = LastPing::Never;
};

/// Builds the text: one line, keys in the order schema, service_version, level, model_generated_utc,
/// last_report_utc, last_send_utc, last_send_result, queued, server_url, server_key_set, server_enabled,
/// device_type, interval_seconds, idle_threshold_seconds, unused_threshold_seconds, settings_source ("server" or
/// "local"), settings_version (only for the server source and when known), last_settings_utc, last_ping_utc (both only
/// when known), last_ping_result, ending with a line feed.
/// The settings fields are always written, so the client can show them without reading config (closed to users).
/// @param fields The values.
/// @return The JSON text.
std::string BuildStatusJson(const StatusFields& fields);

}  // namespace ews
