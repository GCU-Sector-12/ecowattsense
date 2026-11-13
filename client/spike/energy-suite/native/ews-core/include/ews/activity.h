// activity.h: the pure rules of the session pipe (SPEC section 6): idle seconds, the state, and the message parser.
// Used by ews-service when it builds a report and when it reads a pipe message. No pipe or Windows call is made here.
// Pipe bytes are untrusted: the parser accepts one exact format and rejects everything else.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "ews/reading.h"

namespace ews {

/// A message is fresh when it is younger than this many seconds.
constexpr std::int64_t kFreshMessageSeconds = 120;

/// The longest pipe message, in bytes.
constexpr std::size_t kPipeMessageMaxBytes = 128;

/// One parsed pipe message.
struct PipeMessage {
    std::int64_t idleSeconds = 0;        ///< 0 to 2147483647.
    std::optional<int> brightnessPct;    ///< 0 to 100; empty when the client sent "-".
};

/// Parses `EWS1 idle=<seconds> bright=<0-100 or ->\n`: at most 128 bytes, ASCII only, exact keys and spaces,
/// numbers without sign or leading zeros, idle 0 to 2147483647, brightness 0 to 100, one final line feed.
/// @param bytes The message as received.
/// @return Ok with the message, or Error (ERROR_INVALID_DATA) for anything else.
Reading<PipeMessage> ParsePipeMessage(std::string_view bytes);

/// The last message the service accepted.
struct SessionActivity {
    bool hasMessage = false;
    std::int64_t idleSeconds = 0;    ///< The idle value of the message.
    std::int64_t receivedUnix = 0;   ///< When the service received it, Unix seconds.
};

/// Idle seconds of a report. With a fresh message (younger than 120 seconds): its idle value plus its age.
/// Else the seconds since the last message, or since the service start when there was none.
/// A clock that went back gives 0 for the age. The result is limited to 0 to 2147483647.
/// @param last The last accepted message.
/// @param nowUnix Now, Unix seconds.
/// @param serviceStartUnix When the service started, Unix seconds.
/// @return The idle seconds.
std::int64_t ComputeIdleSeconds(const SessionActivity& last, std::int64_t nowUnix, std::int64_t serviceStartUnix);

/// The state of the user: 2 (active) below idleThreshold, 1 (idle) below unusedThreshold, else 0 (unused).
/// Nobody logged on gives 0.
/// @param idleSeconds The idle seconds.
/// @param idleThresholdSeconds idle_threshold_seconds of service.ini.
/// @param unusedThresholdSeconds unused_threshold_seconds of service.ini.
/// @param nobodyLoggedOn True when no session is logged on.
/// @return 2, 1 or 0.
int ComputeState(std::int64_t idleSeconds, std::int64_t idleThresholdSeconds, std::int64_t unusedThresholdSeconds,
                 bool nobodyLoggedOn);

}  // namespace ews
