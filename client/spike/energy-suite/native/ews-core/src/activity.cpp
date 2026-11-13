// activity.cpp: the pipe rules (see ews/activity.h and SPEC section 6).
// Used by ews-service. The message parser is strict: one exact format, everything else is rejected.
#include "ews/activity.h"

#include <algorithm>
#include <charconv>
#include <system_error>

#include <windows.h>

namespace ews {

namespace {

constexpr std::int64_t kMaxIdleSeconds = 2147483647;
constexpr int kMaxBrightness = 100;
constexpr std::size_t kMaxNumberDigits = 10;

/// @return True when the byte is below 0x80.
bool IsAscii(char c) { return static_cast<unsigned char>(c) < 0x80; }

/// @return True for 0 to 9.
bool IsDigit(char c) { return c >= '0' && c <= '9'; }

/// @return The failure for a message that is not accepted.
Reading<PipeMessage> Rejected() { return Reading<PipeMessage>::Failure(Status::Error, ERROR_INVALID_DATA); }

/// Removes `prefix` from the front of `text` when it is there.
/// @return True when it was removed.
bool ConsumePrefix(std::string_view& text, std::string_view prefix) {
    if (text.substr(0, prefix.size()) != prefix) {
        return false;
    }
    text.remove_prefix(prefix.size());
    return true;
}

/// Reads a number from the front of `text`: digits only, no sign, no leading zero (except 0 itself), at most `limit`.
/// @return True when a valid number was read and removed from `text`.
bool ConsumeNumber(std::string_view& text, std::int64_t limit, std::int64_t& value) {
    auto end = std::find_if_not(text.begin(), text.end(), IsDigit);
    std::size_t digits = static_cast<std::size_t>(end - text.begin());
    if (digits == 0 || digits > kMaxNumberDigits) {
        return false;
    }
    if (digits > 1 && text[0] == '0') {
        return false;
    }
    auto converted = std::from_chars(text.data(), text.data() + digits, value);
    if (converted.ec != std::errc() || value > limit) {
        return false;
    }
    text.remove_prefix(digits);
    return true;
}

/// Reads the brightness: "-" for unknown, else a number from 0 to 100.
bool ConsumeBrightness(std::string_view& text, std::optional<int>& brightness) {
    if (ConsumePrefix(text, "-")) {
        brightness.reset();
        return true;
    }
    std::int64_t value = 0;
    if (!ConsumeNumber(text, kMaxBrightness, value)) {
        return false;
    }
    brightness = static_cast<int>(value);
    return true;
}

}  // namespace

Reading<PipeMessage> ParsePipeMessage(std::string_view bytes) {
    if (bytes.size() > kPipeMessageMaxBytes || !std::all_of(bytes.begin(), bytes.end(), IsAscii)) {
        return Rejected();
    }
    PipeMessage message;
    std::string_view rest = bytes;
    if (!ConsumePrefix(rest, "EWS1 idle=") || !ConsumeNumber(rest, kMaxIdleSeconds, message.idleSeconds)) {
        return Rejected();
    }
    if (!ConsumePrefix(rest, " bright=") || !ConsumeBrightness(rest, message.brightnessPct)) {
        return Rejected();
    }
    if (rest != "\n") {
        return Rejected();
    }
    return Reading<PipeMessage>::Success(message);
}

std::int64_t ComputeIdleSeconds(const SessionActivity& last, std::int64_t nowUnix, std::int64_t serviceStartUnix) {
    std::int64_t since = last.hasMessage ? last.receivedUnix : serviceStartUnix;
    std::int64_t age = std::max<std::int64_t>(0, nowUnix - since);
    bool fresh = last.hasMessage && nowUnix >= last.receivedUnix && age < kFreshMessageSeconds;
    std::int64_t idle = fresh ? last.idleSeconds + age : age;
    return std::clamp<std::int64_t>(idle, 0, kMaxIdleSeconds);
}

int ComputeState(std::int64_t idleSeconds, std::int64_t idleThresholdSeconds, std::int64_t unusedThresholdSeconds,
                 bool nobodyLoggedOn) {
    if (nobodyLoggedOn) {
        return 0;
    }
    if (idleSeconds < idleThresholdSeconds) {
        return 2;
    }
    return idleSeconds < unusedThresholdSeconds ? 1 : 0;
}

}  // namespace ews
