// smbios.cpp: pure SMBIOS parser and chassis mapping (see ews/machine_info.h).
// Used by SystemMachineInfoSource and by the tests. It works on a byte span only, so it is safe to test with fixtures.
// The bytes come from firmware and are untrusted: every offset is range-checked before it is read.
// Only type 1 (maker, product name) and type 3 (chassis type) are read. Serial number and UUID are never touched.
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "ews/machine_info.h"

namespace ews {

namespace {

constexpr std::size_t kRawHeaderSize = 8;       // RawSMBIOSData header before the structures
constexpr std::size_t kStructureHeaderSize = 4; // type, length, handle (2 bytes)
constexpr std::size_t kMaxTextLength = 64;      // longest maker or model text that is kept
constexpr std::uint8_t kTypeSystem = 1;
constexpr std::uint8_t kTypeChassis = 3;
constexpr std::uint8_t kTypeEnd = 127;
constexpr std::size_t kType1ProductOffset = 5;  // last field of type 1 that is read
constexpr std::size_t kType3ChassisOffset = 5;
constexpr std::uint8_t kChassisMask = 0x7F;     // bit 7 is the chassis lock flag

/// The parse position and the result, passed between the small steps below.
struct ParseState {
    std::span<const std::uint8_t> table;
    std::size_t position = 0;
    SmbiosInfo info;
};

/// Finds the end of a string set: the first pair of zero bytes at or after `start`.
/// @param table The table bytes.
/// @param start Index of the first byte of the string set.
/// @return The index of the first zero of the pair, or npos when the pair is missing.
std::size_t FindStringSetEnd(std::span<const std::uint8_t> table, std::size_t start) {
    std::size_t i = start;
    while (i + 1 < table.size() && !(table[i] == 0 && table[i + 1] == 0)) {
        ++i;
    }
    return i + 1 < table.size() ? i : std::string::npos;
}

/// Keeps only printable ASCII and limits the length, because firmware text is untrusted.
/// @param raw The bytes of one string.
/// @return The cleaned text; empty when nothing printable is left.
std::string CleanText(std::string_view raw) {
    std::string out;
    for (char c : raw.substr(0, kMaxTextLength)) {
        bool printable = c >= 0x20 && c < 0x7F;
        out.push_back(printable ? c : ' ');
    }
    std::size_t first = out.find_first_not_of(' ');
    std::size_t last = out.find_last_not_of(' ');
    return first == std::string::npos ? std::string() : out.substr(first, last - first + 1);
}

/// Splits a string set into its strings.
/// @param table The table bytes.
/// @param begin Index of the first byte of the set.
/// @param end Index where the set stops (the first zero of the closing pair).
/// @return The strings in order; string number 1 is element 0.
std::vector<std::string> SplitStrings(std::span<const std::uint8_t> table, std::size_t begin, std::size_t end) {
    std::vector<std::string> strings;
    std::size_t cursor = begin;
    while (cursor < end) {
        auto from = table.begin() + static_cast<std::ptrdiff_t>(cursor);
        auto limit = table.begin() + static_cast<std::ptrdiff_t>(end);
        auto zero = std::find(from, limit, std::uint8_t{0});
        strings.emplace_back(from, zero);
        cursor = static_cast<std::size_t>(zero - table.begin()) + 1;
    }
    return strings;
}

/// Looks up one string of a string set by its 1-based index.
/// @param strings The split string set.
/// @param index The index byte from the formatted area; 0 means "no string".
/// @return The cleaned text, or nothing when the index is 0, beyond the set, or the text is empty.
std::optional<std::string> StringAt(const std::vector<std::string>& strings, std::uint8_t index) {
    if (index == 0 || index > strings.size()) {
        return std::nullopt;
    }
    std::string text = CleanText(strings[index - 1]);
    if (text.empty()) {
        return std::nullopt;
    }
    return text;
}

/// Reads maker and product name from a type 1 structure.
/// @param state The parse state; info.maker and info.model are set when found.
/// @param length The formatted length of the structure.
/// @param strings The split string set.
void ReadType1(ParseState& state, std::size_t length, const std::vector<std::string>& strings) {
    if (length <= kType1ProductOffset || state.info.maker || state.info.model) {
        return;  // too short to hold the fields, or a second type 1 (the first one wins)
    }
    state.info.maker = StringAt(strings, state.table[state.position + 4]);
    state.info.model = StringAt(strings, state.table[state.position + kType1ProductOffset]);
}

/// Reads the chassis type byte from a type 3 structure (the first one wins).
/// @param state The parse state; info.chassisType is set when found.
/// @param length The formatted length of the structure.
void ReadType3(ParseState& state, std::size_t length) {
    if (length <= kType3ChassisOffset || state.info.chassisType) {
        return;
    }
    state.info.chassisType = static_cast<std::uint8_t>(state.table[state.position + kType3ChassisOffset] & kChassisMask);
}

/// Parses one structure at the current position and moves to the next.
/// @param state The parse state.
/// @return False when parsing must stop: end of table, end marker, a bad length, or a missing terminator.
bool ParseNext(ParseState& state) {
    const auto& table = state.table;
    if (state.position + kStructureHeaderSize > table.size()) {
        return false;
    }
    std::uint8_t type = table[state.position];
    std::size_t length = table[state.position + 1];
    if (length < kStructureHeaderSize || state.position + length > table.size() || type == kTypeEnd) {
        return false;
    }
    std::size_t stringsBegin = state.position + length;
    std::size_t stringsEnd = FindStringSetEnd(table, stringsBegin);
    if (stringsEnd == std::string::npos) {
        return false;  // no terminator: the strings cannot be trusted
    }
    std::vector<std::string> strings = SplitStrings(table, stringsBegin, stringsEnd);
    if (type == kTypeSystem) {
        ReadType1(state, length, strings);
    }
    if (type == kTypeChassis) {
        ReadType3(state, length);
    }
    state.position = stringsEnd + 2;
    return true;
}

}  // namespace

std::span<const std::uint8_t> ExtractSmbiosTable(std::span<const std::uint8_t> raw) {
    if (raw.size() < kRawHeaderSize) {
        return {};
    }
    std::size_t declared = static_cast<std::size_t>(raw[4]) | (static_cast<std::size_t>(raw[5]) << 8) |
                           (static_cast<std::size_t>(raw[6]) << 16) | (static_cast<std::size_t>(raw[7]) << 24);
    std::size_t available = raw.size() - kRawHeaderSize;
    return raw.subspan(kRawHeaderSize, std::min(declared, available));
}

SmbiosInfo ParseSmbios(std::span<const std::uint8_t> table) {
    ParseState state;
    state.table = table;
    while (ParseNext(state)) {
        // each step moves the position forward; ParseNext stops at the end or on bad data
    }
    return state.info;
}

const char* ChassisWord(std::uint8_t chassisType) {
    switch (chassisType & kChassisMask) {
        case 8: case 9: case 10: case 11: case 14: case 30: case 31: case 32:
            return "laptop";
        case 3: case 4: case 5: case 6: case 7: case 13: case 35: case 36:
            return "desktop";
        default:
            return "other";
    }
}

}  // namespace ews
