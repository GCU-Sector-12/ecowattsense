// config.cpp: service.ini parsing, the URL and device type rules, and the ini writer (see ews/config.h).
// Used by ews-service and by the configure input.
#include "ews/config.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <vector>

#include <windows.h>

#include "ews/atomic_file.h"
#include "ews/file_io.h"
#include "ews/line_text.h"

namespace ews {

namespace {

constexpr std::size_t kMaxUrlLength = 300;
constexpr std::size_t kMaxDeviceTypeLength = 40;
constexpr int kDefaultIdle = 600;
constexpr int kDefaultUnused = 3600;

using Values = std::map<std::string, std::string>;

/// Splits text at one character and keeps empty parts ("a..b" gives three parts).
std::vector<std::string> SplitOn(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    std::size_t end = text.find(separator);
    while (end != std::string::npos) {
        parts.push_back(text.substr(start, end - start));
        start = end + 1;
        end = text.find(separator, start);
    }
    parts.push_back(text.substr(start));
    return parts;
}

/// @return True for a letter or digit of ASCII.
bool IsAlnum(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); }

/// @return True for a character of a host name label.
bool IsLabelChar(char c) { return IsAlnum(c) || c == '-'; }

/// @return True for a label of 1 to 63 characters that does not start or end with a hyphen.
bool IsValidLabel(const std::string& label) {
    bool lengthOk = !label.empty() && label.size() <= 63;
    bool edgesOk = lengthOk && label.front() != '-' && label.back() != '-';
    return edgesOk && std::all_of(label.begin(), label.end(), IsLabelChar);
}

/// @return True for a host name or IPv4 address of letters, digits, dots and hyphens in valid labels.
bool IsValidHostName(const std::string& host) {
    if (host.empty() || host.size() > 253) {
        return false;
    }
    std::vector<std::string> labels = SplitOn(host, '.');
    return std::all_of(labels.begin(), labels.end(), IsValidLabel);
}

/// @return True for 1 to 5 digits that make a port from 1 to 65535.
bool IsValidPort(const std::string& port) {
    std::optional<int> number = port.size() <= 5 ? ParseWholeNumber(port) : std::nullopt;
    return number.has_value() && *number >= 1 && *number <= 65535;
}

/// @return The lower case of one character (ASCII letters only).
char LowerChar(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

/// @return The text in lower case (ASCII).
std::string Lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), LowerChar);
    return text;
}

/// @return True for the three hosts that may use plain http.
bool IsLoopbackHost(const std::string& host) { return host == "localhost" || host == "127.0.0.1" || host == "[::1]"; }

/// Splits "host[:port]" into host and port text. The port text is empty when there is no port.
/// @return False when the shape is wrong (a colon without a port, or characters after a bracketed host).
bool SplitHostPort(const std::string& authority, std::string& host, std::string& port, bool& hasPort) {
    bool bracketed = !authority.empty() && authority.front() == '[';
    std::size_t colon = bracketed ? authority.find("]:") : authority.find(':');
    std::size_t split = (bracketed && colon != std::string::npos) ? colon + 1 : colon;
    hasPort = split != std::string::npos;
    host = hasPort ? authority.substr(0, split) : authority;
    port = hasPort ? authority.substr(split + 1) : std::string();
    return !hasPort || !port.empty();
}

/// Reads the start of a URL. @return The text after the scheme and sets `secure`, or an empty text for a bad scheme.
std::string StripScheme(const std::string& url, bool& secure) {
    const std::string https = "https://";
    const std::string http = "http://";
    secure = url.compare(0, https.size(), https) == 0;
    bool plain = url.compare(0, http.size(), http) == 0;
    std::size_t skip = secure ? https.size() : (plain ? http.size() : 0);
    return skip == 0 ? std::string() : url.substr(skip);
}

/// @return True for a character that is a printable ASCII character with no blank (so no control character either).
bool IsVisibleAscii(char c) { return c > 0x20 && c < 0x7f; }

/// @return True for a character of a device type after its first one.
bool IsTypeChar(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; }

/// Finds a value of the map. @return The value, or nullptr when the key is missing.
const std::string* Find(const Values& values, const char* key) {
    auto found = values.find(key);
    return found == values.end() ? nullptr : &found->second;
}

/// Stores the `key=value` of one line (both trimmed) into the map; a line without `=` is ignored. The last one wins.
void StoreLine(Values& values, const std::string& line) {
    std::size_t equals = line.find('=');
    if (equals == std::string::npos) {
        return;
    }
    values[TrimBlanks(line.substr(0, equals))] = TrimBlanks(line.substr(equals + 1));
}

/// Reads `server_enabled` (0 or 1).
void ApplyEnabled(const Values& values, ServiceConfig& config) {
    const std::string* value = Find(values, "server_enabled");
    if (value == nullptr) {
        return;
    }
    bool good = *value == "0" || *value == "1";
    config.serverEnabled = good ? *value == "1" : config.serverEnabled;
    config.badValues += good ? 0 : 1;
}

/// Reads `server_url`; a bad URL leaves the URL empty and is counted. A missing key or a blank value
/// (values are already trimmed) means "server not set": the URL stays empty and nothing is counted.
void ApplyUrl(const Values& values, ServiceConfig& config) {
    const std::string* value = Find(values, "server_url");
    if (value == nullptr || value->empty()) {
        return;
    }
    bool good = IsValidServerUrl(*value);
    config.serverUrl = good ? *value : std::string();
    config.badValues += good ? 0 : 1;
}

/// Reads a whole number key and checks it with a range.
/// @return The number, or the default when the key is missing or out of range; a present bad value is counted.
int ReadRanged(const Values& values, const char* key, int low, int high, int fallback, int& bad) {
    const std::string* value = Find(values, key);
    if (value == nullptr) {
        return fallback;
    }
    std::optional<int> number = ParseWholeNumber(*value);
    bool good = number.has_value() && *number >= low && *number <= high;
    bad += good ? 0 : 1;
    return good ? *number : fallback;
}

/// Reads `interval_seconds`: a multiple of 60 from 60 to 3600.
void ApplyInterval(const Values& values, ServiceConfig& config) {
    int before = config.badValues;
    int interval = ReadRanged(values, "interval_seconds", 60, 3600, config.intervalSeconds, config.badValues);
    bool multiple = interval % 60 == 0;
    config.intervalSeconds = multiple ? interval : ServiceConfig().intervalSeconds;
    config.badValues += (multiple || config.badValues != before) ? 0 : 1;
}

/// Reads the two thresholds. The unused threshold must be above the idle threshold; when it is not, both
/// go back to their defaults as far as needed and the pair is counted once.
void ApplyThresholds(const Values& values, ServiceConfig& config) {
    config.idleThresholdSeconds = ReadRanged(values, "idle_threshold_seconds", 60, 86400, kDefaultIdle, config.badValues);
    config.unusedThresholdSeconds = ReadRanged(values, "unused_threshold_seconds", 1, 604800, kDefaultUnused, config.badValues);
    if (config.unusedThresholdSeconds > config.idleThresholdSeconds) {
        return;
    }
    config.badValues += 1;
    config.unusedThresholdSeconds = kDefaultUnused;
    config.idleThresholdSeconds = kDefaultUnused > config.idleThresholdSeconds ? config.idleThresholdSeconds : kDefaultIdle;
}

/// Reads `device_type`.
void ApplyDeviceType(const Values& values, ServiceConfig& config) {
    const std::string* value = Find(values, "device_type");
    if (value == nullptr) {
        return;
    }
    bool good = IsValidDeviceType(*value);
    config.deviceType = good ? *value : config.deviceType;
    config.badValues += good ? 0 : 1;
}

/// Replaces a line when its key is the wanted one. @return The new line, or the old line.
std::string ReplaceIfKey(const std::string& line, const std::string& key, const std::string& value, bool& found) {
    std::size_t equals = line.find('=');
    bool same = equals != std::string::npos && TrimBlanks(line.substr(0, equals)) == key;
    found = found || same;
    return same ? key + "=" + value : line;
}

}  // namespace

std::optional<ServerEndpoint> ParseServerUrl(const std::string& url) {
    bool lengthOk = !url.empty() && url.size() <= kMaxUrlLength && std::all_of(url.begin(), url.end(), IsVisibleAscii);
    ServerEndpoint endpoint;
    std::string authority = lengthOk ? StripScheme(url, endpoint.secure) : std::string();
    std::string port;
    bool hasPort = false;
    bool shapeOk = !authority.empty() && SplitHostPort(authority, endpoint.host, port, hasPort);
    endpoint.host = Lower(endpoint.host);
    bool hostOk = shapeOk && (endpoint.host == "[::1]" || IsValidHostName(endpoint.host));
    bool portOk = hostOk && (!hasPort || IsValidPort(port));
    bool schemeOk = portOk && (endpoint.secure || IsLoopbackHost(endpoint.host));
    if (!schemeOk) {
        return std::nullopt;
    }
    endpoint.port = hasPort ? *ParseWholeNumber(port) : (endpoint.secure ? 443 : 80);
    return endpoint;
}

bool IsValidServerUrl(const std::string& url) { return ParseServerUrl(url).has_value(); }

bool IsValidDeviceType(const std::string& type) {
    bool lengthOk = !type.empty() && type.size() <= kMaxDeviceTypeLength;
    bool firstOk = lengthOk && type.front() >= 'a' && type.front() <= 'z';
    return firstOk && std::all_of(type.begin() + 1, type.end(), IsTypeChar);
}

ServiceConfig ParseServiceConfig(const std::string& text) {
    Values values;
    for (const std::string& line : SplitLines(text)) {
        StoreLine(values, line);
    }
    ServiceConfig config;
    ApplyEnabled(values, config);
    ApplyUrl(values, config);
    ApplyInterval(values, config);
    ApplyThresholds(values, config);
    ApplyDeviceType(values, config);
    return config;
}

Reading<ServiceConfig> LoadServiceConfig(const std::wstring& dataDir) {
    std::filesystem::path file = std::filesystem::path(dataDir) / L"config" / L"service.ini";
    Reading<std::string> text = ReadWholeFile(file, kMaxConfigBytes);
    if (!text.ok()) {
        return Reading<ServiceConfig>::Failure(text.status, text.error);
    }
    return Reading<ServiceConfig>::Success(ParseServiceConfig(text.value));
}

std::string SetIniValue(const std::string& text, const std::string& key, const std::string& value) {
    std::vector<std::string> lines = SplitLines(text);
    bool found = false;
    for (std::string& line : lines) {
        line = ReplaceIfKey(line, key, value, found);
    }
    if (!found) {
        lines.push_back(key + "=" + value);
    }
    std::string result;
    for (const std::string& line : lines) {
        result += line + "\n";
    }
    return result;
}

Reading<bool> SaveServerUrl(const std::wstring& dataDir, const std::string& url) {
    using Result = Reading<bool>;
    Reading<bool> folder = EnsureSubfolder(dataDir, L"config");
    if (!folder.ok()) {
        return folder;
    }
    std::filesystem::path configFolder = std::filesystem::path(dataDir) / L"config";
    Reading<std::string> old = ReadWholeFile(configFolder / L"service.ini", kMaxConfigBytes);
    // Only a missing file may be started again; any other failure keeps the old file as it is.
    if (!old.ok() && old.status != Status::Unavailable) {
        return Result::Failure(old.status, old.error);
    }
    std::string text = SetIniValue(old.ok() ? old.value : std::string(), "server_url", url);
    if (text.size() > kMaxConfigBytes) {
        return Result::Failure(Status::Error, ERROR_FILE_TOO_LARGE);
    }
    return WriteFileAtomically(configFolder.wstring(), L"service.ini", text);
}

}  // namespace ews
