// device_name.h: the pure rules of the computer name the client sends to the service (SPEC 5.5, 5.7 and 6).
// Used by ServiceCore (the pipe message `EWS1 name=<base64>`, the state file name-sync.json and the answer of the
// PUT to the server) and by the tests. No file, pipe or network call is made here. Everything that comes in is
// untrusted: pipe bytes, the state file and the server answer are checked before use.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "ews/reading.h"

namespace ews {

/// The longest name message, in bytes (the other pipe messages stay at 128).
constexpr std::size_t kNameMessageMaxBytes = 256;

/// The least time between two accepted name messages, in seconds.
constexpr long long kNameMessageGapSeconds = 10;

/// Where the name stands with the server (SPEC 5.7 `name_sync`).
enum class NameSync {
    None,     ///< No rename and no PUT yet; the status field is left out.
    Ok,       ///< The last PUT was answered 200.
    Pending,  ///< The last PUT did not reach the server (or none was possible yet); it is tried again on the ping tick.
    Failed    ///< The server refused (401 or 422); it is not tried again.
};

/// @param text The value of the pipe message after the prefix.
/// @return True when the bytes start a name message (`EWS1 name=`).
bool IsNameMessage(std::string_view bytes);

/// Parses `EWS1 name=<base64 of the UTF-8 name>\n`: at most 256 bytes, strict base64 with padding, a line feed at the
/// end. An empty value means "no name". The decoded name is trimmed of spaces and must be well-formed UTF-8 of 1 to
/// 60 characters (code points) without control characters.
/// @param bytes The message as received.
/// @return Ok with the name (nothing for "no name"), or Error (ERROR_INVALID_DATA) for anything else.
Reading<std::optional<std::string>> ParseNameMessage(std::string_view bytes);

/// Decodes strict base64 (standard alphabet, `=` padding, no white space, unused bits zero).
/// @param text The text.
/// @return The bytes, or nothing when the text is empty or not exactly base64.
std::optional<std::string> DecodeBase64(std::string_view text);

/// Checks a name that came from outside.
/// @param name UTF-8 bytes.
/// @return The name without spaces at both ends, or nothing when it is empty, not well-formed UTF-8, longer than 60
///         characters or holds a control character.
std::optional<std::string> CheckedDeviceName(const std::string& name);

/// @param state A state.
/// @return A fixed word: `ok`, `pending`, `failed`; the empty text for None.
const char* NameSyncWord(NameSync state);

/// Writes name-sync.json.
/// @param state The state to remember.
/// @return One JSON line `{"name_sync":"<word>"}` and a line feed.
std::string FormatNameSync(NameSync state);

/// Reads name-sync.json.
/// @param text The file content.
/// @return The state; None for a broken text or an unknown word.
NameSync ParseNameSync(const std::string& text);

/// The name part of the answer to `PUT /api/devices/{device_id}`.
struct DeviceAnswer {
    std::optional<std::string> name;  ///< Empty when the server holds no name (JSON null).
};

/// Reads the answer of the PUT.
/// @param text The body.
/// @return Ok with the name the server holds now; Error when the body is not a JSON object with `device_name` as a
///         string or null.
Reading<DeviceAnswer> ParseDeviceAnswer(const std::string& text);

}  // namespace ews
