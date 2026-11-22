// config.h: reads and writes `<data>\config\service.ini` (SPEC 5.6).
// Used by ews-service at start and by the configure input (configure.h).
// The file is `key=value` lines. Unknown keys are ignored and the file is at most 16 KB.
// A value that is present but wrong falls back to its default and is counted, never trusted.
#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "ews/reading.h"

namespace ews {

/// The largest accepted size of service.ini.
inline constexpr std::size_t kMaxConfigBytes = 16 * 1024;

/// The settings of service.ini after checking. Every field holds a valid value.
struct ServiceConfig {
    bool serverEnabled = true;          ///< `server_enabled`: 0 or 1.
    std::string serverUrl;              ///< `server_url`: empty when missing or bad.
    int intervalSeconds = 300;          ///< `interval_seconds`: a multiple of 60, 60 to 3600.
    int idleThresholdSeconds = 600;     ///< `idle_threshold_seconds`: 60 to 86400.
    int unusedThresholdSeconds = 3600;  ///< `unused_threshold_seconds`: above the idle threshold, at most 604800.
    std::string deviceType = "laptop";  ///< `device_type`: `^[a-z][a-z0-9_]*$`, at most 40 characters.
    int badValues = 0;                  ///< How many keys were present with a wrong value.

    /// @return True when reports may be sent: sending is enabled and a valid URL is set.
    bool SendingOn() const { return serverEnabled && !serverUrl.empty(); }
};

/// The parts of a checked server URL.
struct ServerEndpoint {
    std::string host;     ///< Lower case host name or address; `[::1]` keeps its brackets.
    int port = 443;       ///< 1 to 65535; 443 or 80 when the URL has no port.
    bool secure = true;   ///< True for `https`.
};

/// Checks a server URL and splits it: `https://host[:port]`, or `http://` only for `localhost`, `127.0.0.1` and `[::1]`.
/// No path (not even `/`), query, fragment, user info or blank is allowed. The port is 1 to 65535.
/// A host is letters, digits, dots and hyphens in non-empty labels; `[::1]` is the only accepted IPv6 form.
/// @param url The text to check; at most 300 characters.
/// @return The parts, or nothing when the URL breaks the rule.
std::optional<ServerEndpoint> ParseServerUrl(const std::string& url);

/// Checks a server URL with the rule of ParseServerUrl.
/// @param url The text to check.
/// @return True when the URL follows the rule.
bool IsValidServerUrl(const std::string& url);

/// Checks a device type: `^[a-z][a-z0-9_]*$`, at most 40 characters.
/// @param type The text to check.
/// @return True when the text follows the rule.
bool IsValidDeviceType(const std::string& type);

/// Parses the text of service.ini. Never fails: a missing key keeps its default, a wrong value falls back to
/// its default and raises `badValues`. A key that appears twice takes its last value.
/// @param text The file content.
/// @return The checked settings.
ServiceConfig ParseServiceConfig(const std::string& text);

/// Reads `<data>\config\service.ini`.
/// @param dataDir The data folder.
/// @return Ok with the settings. When the file is missing, too large or unreadable the status says why and the
///         value holds the defaults, so the caller can still run (sending is off because no URL is set).
Reading<ServiceConfig> LoadServiceConfig(const std::wstring& dataDir);

/// Sets one key in ini text and keeps every other line as it is. Existing lines of that key are replaced;
/// when there is none the line is appended.
/// @param text The old content (may be empty).
/// @param key The key, without `=`.
/// @param value The new value; it must hold no line break.
/// @return The new content.
std::string SetIniValue(const std::string& text, const std::string& key, const std::string& value);

/// Writes the `server_url` key into `<data>\config\service.ini`, keeping the other lines. The file is created when missing.
/// @param dataDir The data folder.
/// @param url A URL that passed IsValidServerUrl.
/// @return Ok with true. Failure when the file exists but cannot be read (it is then not overwritten), when the
///         result would be over 16 KB, or when the write fails.
Reading<bool> SaveServerUrl(const std::wstring& dataDir, const std::string& url);

}  // namespace ews
