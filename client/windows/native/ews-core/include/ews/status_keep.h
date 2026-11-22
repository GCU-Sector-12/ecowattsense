// status_keep.h: reads the three fields of an old status.json that the service keeps across a restart (SPEC 5.7).
// Used by ServiceCore::Start, so the first status after a restart still shows the last report and the last send.
// The file is untrusted text (the client folder is not writable by users, but a broken disk or an old version can
// leave anything), so every field is checked on its own and a bad one is left out.
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "ews/status_format.h"

namespace ews {

/// The fields that survive a restart. An empty field was missing or not valid.
struct KeptStatus {
    std::optional<std::int64_t> lastReportUnix;
    std::optional<std::int64_t> lastSendUnix;
    std::optional<LastSend> lastSendResult;
};

/// The most a kept time may lie in the future of the clock, in seconds (clock differences, never a real future).
inline constexpr std::int64_t kKeptTimeSlackSeconds = 5 * 60;

/// The largest status file that is read, in bytes.
inline constexpr std::size_t kMaxStatusFileBytes = 64 * 1024;

/// Maps a status word back to its value.
/// @param word Untrusted text.
/// @return The result for one of the seven words of SPEC 5.7, or nothing for any other text.
std::optional<LastSend> SendResultFromWord(const std::string& word);

/// Reads `<data>\status\status.json` and keeps the valid fields.
/// @param dataDir The data folder.
/// @param nowUnix The current time, Unix seconds; a kept time later than now plus 5 minutes is not valid.
/// @return The valid fields. A missing, too large (over 64 KB) or broken file, or a field that is missing or not
///         valid, gives an empty field; nothing is thrown and nothing is written.
KeptStatus ReadKeptStatus(const std::wstring& dataDir, std::int64_t nowUnix);

}  // namespace ews
