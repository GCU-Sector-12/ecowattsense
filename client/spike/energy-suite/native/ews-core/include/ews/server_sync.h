// server_sync.h: the pure parts of the ping, the server settings and the commands (SPEC 5.5, api-contract 5.2, 5.3, 5.5).
// Used by ServiceCore (service_sync.cpp) and the tests. Nothing here touches the network or a file.
// Every number that comes from the server or from a file is untrusted and range-checked here.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ews/reading.h"

namespace ews {

inline constexpr std::size_t kMaxSettingsFileBytes = 16 * 1024;  ///< The largest accepted server-settings.json.
inline constexpr std::size_t kMaxCommandsFileBytes = 16 * 1024;  ///< The largest accepted commands-done.json.
inline constexpr std::size_t kMaxDoneCommands = 200;             ///< The most handled command ids kept in the file.
inline constexpr std::size_t kMaxCommandsPerPing = 50;           ///< A ping answer with more commands is not accepted.
inline constexpr std::size_t kMaxDeviceNameChars = 60;           ///< The longest computer name sent to the server.

/// The settings the server gives (api-contract 5.2), as far as the service uses them.
struct ServerSettings {
    std::optional<std::int64_t> version;      ///< `settings_version`; empty only when the file came from a `set_interval` before any read.
    int reportIntervalSeconds = 300;          ///< 60 to 3600, a whole number of minutes.
    int pingIntervalSeconds = 60;             ///< 60 to 86400, a whole number of minutes.
    int idleThresholdSeconds = 600;           ///< 1 to 86400.
    int unusedThresholdSeconds = 3600;        ///< Above the idle threshold, at most 604800.
    std::optional<std::int64_t> checkedUnix;  ///< `last_settings_utc` of the file: the time of the last good read.
};

/// The result of a command, as the server wants it in `acks`.
enum class CommandResult { Done, Failed, Unsupported };

/// @param result A result.
/// @return `done`, `failed` or `unsupported`.
const char* CommandResultWord(CommandResult result);

/// A command of a ping answer, after checking.
struct ServerCommand {
    std::int64_t id = 0;                        ///< 1 or more.
    std::string type;                           ///< At most 40 characters; the kind is not checked here.
    std::optional<std::int64_t> intervalParam;  ///< `params.report_interval_seconds` when it is a whole number.
    std::int64_t expiresUnix = 0;               ///< `expires_at` as Unix seconds.
};

/// A ping answer after checking.
struct PingAnswer {
    std::int64_t settingsVersion = 0;
    bool registered = true;
    std::vector<ServerCommand> commands;  ///< In the order of `id`. A command with a bad field is left out.
};

/// A handled command that waits to be acked.
struct DoneCommand {
    std::int64_t id = 0;
    CommandResult result = CommandResult::Done;
};

/// Checks a report interval from outside.
/// @param seconds The raw number.
/// @return The interval rounded to whole minutes, or nothing when the number is not in 60 to 3600.
std::optional<int> NormaliseReportInterval(std::int64_t seconds);

/// Checks a ping interval from outside.
/// @param seconds The raw number.
/// @return The interval rounded to whole minutes and at least 60, or nothing when the number is not in 1 to 86400.
std::optional<int> NormalisePingInterval(std::int64_t seconds);

/// Reads the text of `GET /api/settings`, or of server-settings.json (the same field names).
/// @param text Untrusted JSON text.
/// @param versionRequired True for a server answer; false for the file, where a missing version is allowed.
/// @return Ok with the checked settings, or Error when the text is not JSON or ANY value is missing or out of range
///         (the caller then ignores the whole text). `last_settings_utc` is read when it has the exact form.
Reading<ServerSettings> ParseServerSettings(const std::string& text, bool versionRequired);

/// Writes server-settings.json.
/// @param settings The settings in force.
/// @return One JSON line ending with a line feed.
std::string FormatServerSettings(const ServerSettings& settings);

/// Reads the text of the answer to `POST /api/ping`.
/// @param text Untrusted JSON text.
/// @return Ok with the answer, or Error when the text is not JSON, `settings_version`, `registered` or `commands` is
///         missing or wrong, or there are more than 50 commands.
Reading<PingAnswer> ParsePingAnswer(const std::string& text);

/// Builds the body of `POST /api/ping`.
/// @param deviceId The device id (already checked).
/// @param unsentReports The number of reports in the queue.
/// @param acks The handled commands; `acks` is left out when there are none.
/// @return The JSON text, without spaces.
std::string BuildPingBody(const std::string& deviceId, int unsentReports, const std::vector<DoneCommand>& acks);

/// Builds the body of `PUT /api/devices/{device_id}`.
/// @param deviceName The name, or nothing for null.
/// @param deviceType The device type in force.
/// @return The JSON text.
std::string BuildDeviceBody(const std::optional<std::string>& deviceName, const std::string& deviceType);

/// Takes the computer name from the text of device-name.txt.
/// @param fileText The file content.
/// @return The first line without blanks at both ends and cut to 60 characters, or nothing when it is empty or holds a control character.
std::optional<std::string> DeviceNameFromFile(const std::string& fileText);

/// Reads commands-done.json.
/// @param text The file content.
/// @return The handled commands; a broken text gives an empty list. At most 200 are kept (the newest).
std::vector<DoneCommand> ParseCommandsDone(const std::string& text);

/// Writes commands-done.json.
/// @param done The handled commands.
/// @return One JSON line ending with a line feed.
std::string FormatCommandsDone(const std::vector<DoneCommand>& done);

/// @param done The handled commands.
/// @param id A command id.
/// @return True when the id is in the list.
bool HasDone(const std::vector<DoneCommand>& done, std::int64_t id);

/// Removes the commands that a ping carried as acks and got 200 for, except the ones the answer lists again.
/// @param done The handled commands.
/// @param acked The commands that were in the ping.
/// @param listed The commands of the answer.
/// @return The list that stays.
std::vector<DoneCommand> WithoutAcked(const std::vector<DoneCommand>& done, const std::vector<DoneCommand>& acked,
                                      const std::vector<ServerCommand>& listed);

}  // namespace ews
