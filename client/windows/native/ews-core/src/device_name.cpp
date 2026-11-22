// device_name.cpp: the pure rules of the computer name message (see ews/device_name.h and SPEC 5.5, 5.7 and 6).
// Used by ServiceCore and the tests. Written without nested control statements: every step is a small function.
#include "ews/device_name.h"

#include <algorithm>
#include <cstdint>

#include <windows.h>

#include "ews/json_reader.h"

namespace ews {

namespace {

constexpr std::string_view kNamePrefix = "EWS1 name=";
constexpr std::size_t kMaxNameChars = 60;
constexpr std::string_view kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/// @return The failure for a message that is not accepted.
Reading<std::optional<std::string>> Rejected() {
    return Reading<std::optional<std::string>>::Failure(Status::Error, ERROR_INVALID_DATA);
}

/// @param c One character.
/// @return True when it is a character of the base64 alphabet.
bool IsBase64Char(char c) { return kAlphabet.find(c) != std::string_view::npos; }

/// @param c A character of the alphabet.
/// @return Its value, 0 to 63.
std::uint32_t Base64Value(char c) { return static_cast<std::uint32_t>(kAlphabet.find(c)); }

/// @param text Base64 text.
/// @return The number of `=` at the end, at most 2.
std::size_t PaddingOf(std::string_view text) {
    bool two = text.size() >= 2 && text.substr(text.size() - 2) == "==";
    bool one = text.back() == '=';
    return two ? 2 : (one ? 1 : 0);
}

/// @param lead The first byte of a UTF-8 sequence.
/// @return The length of the sequence, or 0 when the byte cannot start one.
std::size_t SequenceLength(char lead) {
    unsigned char byte = static_cast<unsigned char>(lead);
    std::size_t length = 0;
    length = byte < 0x80 ? 1 : length;
    length = (byte & 0xE0) == 0xC0 ? 2 : length;
    length = (byte & 0xF0) == 0xE0 ? 3 : length;
    length = (byte & 0xF8) == 0xF0 ? 4 : length;
    return length;
}

/// Reads one UTF-8 sequence that is known to fit in the text.
/// @param text The text.
/// @param start The index of the lead byte.
/// @param length The length from SequenceLength (1 to 4).
/// @return The code point, or nothing for a bad continuation byte, an overlong form, a surrogate or a value above 0x10FFFF.
std::optional<std::uint32_t> CodePointAt(const std::string& text, std::size_t start, std::size_t length) {
    static const std::uint32_t kLeadMask[] = {0, 0x7F, 0x1F, 0x0F, 0x07};
    static const std::uint32_t kLeast[] = {0, 0, 0x80, 0x800, 0x10000};
    std::uint32_t point = static_cast<unsigned char>(text[start]) & kLeadMask[length];
    bool continuations = true;
    for (std::size_t index = 1; index < length; ++index) {
        unsigned char next = static_cast<unsigned char>(text[start + index]);
        continuations = continuations && (next & 0xC0) == 0x80;
        point = (point << 6) | (next & 0x3F);
    }
    bool surrogate = point >= 0xD800 && point <= 0xDFFF;
    bool good = continuations && point >= kLeast[length] && point <= 0x10FFFF && !surrogate;
    return good ? std::optional<std::uint32_t>(point) : std::nullopt;
}

/// @param point A code point.
/// @return True for a control character (C0, DEL and C1).
bool IsControlPoint(std::uint32_t point) { return point < 0x20 || (point >= 0x7F && point <= 0x9F); }

/// What a scan of a name found.
struct NameScan {
    bool valid = true;     ///< Well-formed UTF-8.
    bool control = false;  ///< At least one control character.
    std::size_t chars = 0; ///< The number of code points.
};

/// @param text UTF-8 bytes.
/// @return The scan result; counting stops at the first bad sequence.
NameScan ScanName(const std::string& text) {
    NameScan scan;
    std::size_t position = 0;
    while (position < text.size() && scan.valid) {
        std::size_t length = SequenceLength(text[position]);
        bool fits = length > 0 && position + length <= text.size();
        std::optional<std::uint32_t> point = fits ? CodePointAt(text, position, length) : std::nullopt;
        scan.valid = point.has_value();
        scan.control = scan.control || (scan.valid && IsControlPoint(*point));
        scan.chars += scan.valid ? 1 : 0;
        position += length;
    }
    return scan;
}

/// @param text Any text.
/// @return The text without spaces at both ends (tabs are control characters and stay for the check).
std::string TrimSpaces(const std::string& text) {
    std::size_t first = text.find_first_not_of(' ');
    std::size_t last = text.find_last_not_of(' ');
    return first == std::string::npos ? std::string() : text.substr(first, last - first + 1);
}

}  // namespace

bool IsNameMessage(std::string_view bytes) { return bytes.substr(0, kNamePrefix.size()) == kNamePrefix; }

std::optional<std::string> DecodeBase64(std::string_view text) {
    bool shaped = !text.empty() && text.size() % 4 == 0;
    std::size_t padding = shaped ? PaddingOf(text) : 0;
    std::string_view body = text.substr(0, text.size() - padding);
    if (!shaped || !std::all_of(body.begin(), body.end(), IsBase64Char)) {
        return std::nullopt;
    }
    std::string bytes;
    std::uint32_t bits = 0;
    int held = 0;
    for (char c : body) {
        bits = ((bits << 6) | Base64Value(c)) & 0xFFFF;
        held += 6;
        bool full = held >= 8;
        held -= full ? 8 : 0;
        bytes += full ? std::string(1, static_cast<char>((bits >> held) & 0xFF)) : std::string();
    }
    // The bits that are left over must be zero, so one text has one meaning.
    bool canonical = (bits & ((1u << held) - 1)) == 0;
    return canonical ? std::optional<std::string>(bytes) : std::nullopt;
}

std::optional<std::string> CheckedDeviceName(const std::string& name) {
    std::string trimmed = TrimSpaces(name);
    NameScan scan = ScanName(trimmed);
    bool good = !trimmed.empty() && scan.valid && !scan.control && scan.chars <= kMaxNameChars;
    return good ? std::optional<std::string>(trimmed) : std::nullopt;
}

Reading<std::optional<std::string>> ParseNameMessage(std::string_view bytes) {
    bool shaped = bytes.size() <= kNameMessageMaxBytes && IsNameMessage(bytes) && bytes.back() == '\n';
    if (!shaped) {
        return Rejected();
    }
    std::string_view value = bytes.substr(kNamePrefix.size(), bytes.size() - kNamePrefix.size() - 1);
    if (value.empty()) {
        return Reading<std::optional<std::string>>::Success(std::nullopt);
    }
    std::optional<std::string> decoded = DecodeBase64(value);
    std::optional<std::string> name = decoded.has_value() ? CheckedDeviceName(*decoded) : std::nullopt;
    return name.has_value() ? Reading<std::optional<std::string>>::Success(name) : Rejected();
}

const char* NameSyncWord(NameSync state) {
    switch (state) {
        case NameSync::Ok: return "ok";
        case NameSync::Pending: return "pending";
        case NameSync::Failed: return "failed";
        case NameSync::None: break;
    }
    return "";
}

std::string FormatNameSync(NameSync state) { return std::string("{\"name_sync\":\"") + NameSyncWord(state) + "\"}\n"; }

NameSync ParseNameSync(const std::string& text) {
    Reading<JsonValue> root = ParseJson(text);
    const JsonValue* word = root.ok() ? root.value.Find("name_sync") : nullptr;
    bool isText = word != nullptr && word->type == JsonType::String;
    std::string value = isText ? word->text : std::string();
    NameSync found = NameSync::None;
    found = value == "ok" ? NameSync::Ok : found;
    found = value == "pending" ? NameSync::Pending : found;
    found = value == "failed" ? NameSync::Failed : found;
    return found;
}

Reading<DeviceAnswer> ParseDeviceAnswer(const std::string& text) {
    Reading<JsonValue> root = ParseJson(text);
    const JsonValue* name = root.ok() ? root.value.Find("device_name") : nullptr;
    bool isNull = name != nullptr && name->type == JsonType::Null;
    bool isText = name != nullptr && name->type == JsonType::String;
    if (!isNull && !isText) {
        return Reading<DeviceAnswer>::Failure(Status::Error, ERROR_INVALID_DATA);
    }
    DeviceAnswer answer;
    answer.name = isText ? std::optional<std::string>(name->text) : std::nullopt;
    return Reading<DeviceAnswer>::Success(answer);
}

}  // namespace ews
