// json_text.cpp: implementation of the text helpers in ews/json_text.h.
// The JSON string escaping was moved here from scan_output.cpp so the report, history and status
// writers reuse it; the code is unchanged apart from the function name.
#include "ews/json_text.h"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <system_error>

namespace ews {

namespace {

constexpr const char* kHex = "0123456789abcdef";

/// @return True for a UTF-8 continuation byte (10xxxxxx).
bool IsContinuation(unsigned char byte) { return byte >= 0x80 && byte <= 0xBF; }

/// @return The number of bytes of a sequence that starts with this lead byte, or 0 when it cannot start a valid sequence.
std::size_t SequenceLengthOf(unsigned char lead) {
    if (lead >= 0xC2 && lead <= 0xDF) {
        return 2;
    }
    if (lead >= 0xE0 && lead <= 0xEF) {
        return 3;
    }
    return (lead >= 0xF0 && lead <= 0xF4) ? 4 : 0;
}

/// @return True when the second byte is allowed after this lead byte (rules out overlong forms and surrogates).
bool SecondByteAllowed(unsigned char lead, unsigned char second) {
    if (lead == 0xE0) {
        return second >= 0xA0 && second <= 0xBF;
    }
    if (lead == 0xED) {
        return second >= 0x80 && second <= 0x9F;
    }
    if (lead == 0xF0) {
        return second >= 0x90 && second <= 0xBF;
    }
    if (lead == 0xF4) {
        return second >= 0x80 && second <= 0x8F;
    }
    return IsContinuation(second);
}

/// @return True when the character is a UTF-8 continuation byte.
bool IsContinuationChar(char c) { return IsContinuation(static_cast<unsigned char>(c)); }

/// @return True when `count` bytes of `text` from `from` are all continuation bytes. The range must lie inside `text`.
bool ContinuationsFollow(const std::string& text, std::size_t from, std::size_t count) {
    auto begin = text.begin() + static_cast<std::ptrdiff_t>(from);
    return std::all_of(begin, begin + static_cast<std::ptrdiff_t>(count), IsContinuationChar);
}

/// Checks the multi-byte sequence at `pos`.
/// @return Its length in bytes, or 0 when it is not valid UTF-8 (wrong lead, cut short, bad continuation).
std::size_t ValidSequenceLength(const std::string& text, std::size_t pos) {
    auto lead = static_cast<unsigned char>(text[pos]);
    std::size_t length = SequenceLengthOf(lead);
    if (length == 0 || pos + length > text.size()) {
        return 0;
    }
    if (!SecondByteAllowed(lead, static_cast<unsigned char>(text[pos + 1]))) {
        return 0;
    }
    return ContinuationsFollow(text, pos + 2, length - 2) ? length : 0;
}

/// Appends one ASCII character as JSON text: quote and backslash get a backslash, control characters become \u00XX.
void AppendAscii(std::string& out, char c) {
    auto byte = static_cast<unsigned char>(c);
    if (c == '"' || c == '\\') {
        out += '\\';
        out += c;
        return;
    }
    if (byte >= 0x20) {
        out += c;
        return;
    }
    out += "\\u00";
    out += kHex[byte >> 4];
    out += kHex[byte & 0x0F];
}

/// Appends the character that starts at `pos`. Invalid UTF-8 bytes become '?', so the file always parses.
/// @return The number of bytes used from `text`.
std::size_t AppendCharacter(std::string& out, const std::string& text, std::size_t pos) {
    if (static_cast<unsigned char>(text[pos]) < 0x80) {
        AppendAscii(out, text[pos]);
        return 1;
    }
    std::size_t length = ValidSequenceLength(text, pos);
    if (length == 0) {
        out += '?';
        return 1;
    }
    out.append(text, pos, length);
    return length;
}

/// @return The text as a quoted JSON string.
std::string QuoteImpl(const std::string& text) {
    std::string out = "\"";
    std::size_t pos = 0;
    while (pos < text.size()) {
        pos += AppendCharacter(out, text, pos);
    }
    out += '"';
    return out;
}

}  // namespace

std::string JsonQuote(const std::string& text) { return QuoteImpl(text); }

std::string FormatFixed(double value, int decimals) {
    char buffer[512];  // the largest double needs 309 digits before the point
    auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed, decimals);
    if (converted.ec != std::errc()) {
        return std::string();
    }
    return std::string(buffer, converted.ptr);
}

namespace {

/// Splits days since 1970-01-01 into year, month and day (proleptic Gregorian calendar).
void CivilFromDays(std::int64_t days, int& year, int& month, int& day) {
    std::int64_t z = days + 719468;
    std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    std::int64_t dayOfEra = z - era * 146097;
    std::int64_t yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
    std::int64_t dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
    std::int64_t monthShifted = (5 * dayOfYear + 2) / 153;
    day = static_cast<int>(dayOfYear - (153 * monthShifted + 2) / 5 + 1);
    month = static_cast<int>(monthShifted < 10 ? monthShifted + 3 : monthShifted - 9);
    year = static_cast<int>(yearOfEra + era * 400 + (month <= 2 ? 1 : 0));
}

}  // namespace

std::string FormatUtcTime(std::int64_t unixSeconds) {
    constexpr std::int64_t kLastSecond = 253402300799;  // 9999-12-31T23:59:59Z
    std::int64_t clamped = std::clamp<std::int64_t>(unixSeconds, 0, kLastSecond);
    int year = 1970;
    int month = 1;
    int day = 1;
    CivilFromDays(clamped / 86400, year, month, day);
    int secondOfDay = static_cast<int>(clamped % 86400);
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ", year, month, day, secondOfDay / 3600,
                  (secondOfDay % 3600) / 60, secondOfDay % 60);
    return std::string(buffer);
}

}  // namespace ews
