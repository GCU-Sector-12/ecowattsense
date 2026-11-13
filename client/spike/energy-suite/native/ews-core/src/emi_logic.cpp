// emi_logic.cpp: the pure functions of the EMI reader (see ews/emi.h): the metadata parser, the channel
// classifier, the power from two samples and the unit conversion.
// Used by emi.cpp (the real source), by ews-scan and ews-service, and by the tests, which feed it fixture bytes.
// The metadata bytes come from a driver and are untrusted: every offset and size is checked before it is used.
// Layouts follow emi.h of the Windows SDK:
//   version 1: unit(4) OEM(32) model(32) revision(2) nameSize(2) name...
//   version 2: OEM(32) model(32) revision(2) channelCount(2), then per channel: unit(4) nameSize(2) name...
// A name size is in bytes and includes the terminating zero character.

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstring>

#include "ews/emi.h"
#include "ews/text.h"

namespace ews {

namespace {

constexpr std::size_t kV1NameSizeOffset = 70;
constexpr std::size_t kV1NameOffset = 72;
constexpr std::size_t kV2CountOffset = 66;
constexpr std::size_t kV2HeaderBytes = 68;
constexpr std::size_t kV2ChannelFixedBytes = 6;  // unit(4) + nameSize(2)
constexpr std::uint16_t kMaxChannels = 256;
constexpr std::uint32_t kUnitPicowattHours = 0;  // EmiMeasurementUnitPicowattHours
constexpr double kJoulesPerPicowattHour = 3.6e-9;
constexpr double kSecondsPer100ns = 1e-7;

using Meta = Reading<std::vector<EmiChannelMeta>>;

/// @return A failed parse: Error, with no channels.
Meta BadMetadata() { return Meta::Failure(Status::Error, ERROR_INVALID_DATA); }

/// Reads a 16-bit little-endian value. The caller has checked that two bytes exist at the offset.
std::uint16_t ReadU16(std::span<const unsigned char> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
}

/// Reads a 32-bit little-endian value. The caller has checked that four bytes exist at the offset.
std::uint32_t ReadU32(std::span<const unsigned char> bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(ReadU16(bytes, offset)) | (static_cast<std::uint32_t>(ReadU16(bytes, offset + 2)) << 16);
}

/// Where the three fields of one channel sit in the buffer.
struct ChannelLayout {
    std::size_t unit = 0;
    std::size_t nameSize = 0;
    std::size_t name = 0;
};

/// Turns the name bytes into UTF-8 text, up to the first zero character.
/// @param bytes The metadata buffer.
/// @param offset Start of the name; the caller has checked that nameSize bytes exist there.
/// @param nameSize The size in bytes (even, at least 2).
/// @return The name without terminator.
std::string NameFromBytes(std::span<const unsigned char> bytes, std::size_t offset, std::size_t nameSize) {
    std::vector<wchar_t> wide(nameSize / 2);
    std::memcpy(wide.data(), bytes.data() + offset, nameSize);
    auto zero = std::find(wide.begin(), wide.end(), L'\0');
    return WideToUtf8(wide.data(), static_cast<std::size_t>(zero - wide.begin()));
}

/// Checks that a name size field is usable: even, at least 2, and inside the buffer.
bool NameSizeFits(std::span<const unsigned char> bytes, std::size_t nameOffset, std::uint16_t nameSize) {
    bool shapeOk = nameSize >= 2 && nameSize % 2 == 0;
    return shapeOk && nameOffset <= bytes.size() && nameSize <= bytes.size() - nameOffset;
}

/// Parses one channel at the given layout.
/// @param bytes The metadata buffer.
/// @param at The offsets of the unit, the name size and the name.
/// @param end Receives the offset just after the name.
/// @return Ok with the channel, or Error when a field is out of range, the unit is unknown or the name has no terminator.
Reading<EmiChannelMeta> ParseChannel(std::span<const unsigned char> bytes, const ChannelLayout& at, std::size_t& end) {
    using Result = Reading<EmiChannelMeta>;
    if (at.name > bytes.size()) {
        return Result::Failure(Status::Error, ERROR_INVALID_DATA);
    }
    std::uint16_t nameSize = ReadU16(bytes, at.nameSize);
    if (!NameSizeFits(bytes, at.name, nameSize)) {
        return Result::Failure(Status::Error, ERROR_INVALID_DATA);
    }
    bool terminated = ReadU16(bytes, at.name + nameSize - 2) == 0;
    bool unitKnown = ReadU32(bytes, at.unit) == kUnitPicowattHours;
    if (!terminated || !unitKnown) {
        return Result::Failure(Status::Error, ERROR_INVALID_DATA);
    }
    EmiChannelMeta channel;
    channel.name = NameFromBytes(bytes, at.name, nameSize);
    end = at.name + nameSize;
    return Result::Success(std::move(channel));
}

/// Parses the next version 2 channel and adds it to the list.
/// @param bytes The metadata buffer.
/// @param cursor Start of the channel; moved to the start of the next one on success.
/// @param channels The list to extend.
/// @return True when the channel was valid.
bool AppendChannelV2(std::span<const unsigned char> bytes, std::size_t& cursor, std::vector<EmiChannelMeta>& channels) {
    ChannelLayout at{cursor, cursor + 4, cursor + kV2ChannelFixedBytes};
    auto channel = ParseChannel(bytes, at, cursor);
    if (!channel.ok()) {
        return false;
    }
    channels.push_back(std::move(channel.value));
    return true;
}

/// Parses a version 2 buffer.
Meta ParseVersion2(std::span<const unsigned char> bytes) {
    if (bytes.size() < kV2HeaderBytes) {
        return BadMetadata();
    }
    std::uint16_t count = ReadU16(bytes, kV2CountOffset);
    if (count == 0 || count > kMaxChannels) {
        return BadMetadata();
    }
    std::vector<EmiChannelMeta> channels;
    std::size_t cursor = kV2HeaderBytes;
    bool good = true;
    while (good && channels.size() < count) {
        good = AppendChannelV2(bytes, cursor, channels);
    }
    if (!good) {
        return BadMetadata();
    }
    return Meta::Success(std::move(channels));
}

/// Parses a version 1 buffer: one channel after a fixed header.
Meta ParseVersion1(std::span<const unsigned char> bytes) {
    if (bytes.size() < kV1NameOffset) {
        return BadMetadata();
    }
    ChannelLayout at{0, kV1NameSizeOffset, kV1NameOffset};
    std::size_t end = 0;
    auto channel = ParseChannel(bytes, at, end);
    if (!channel.ok()) {
        return BadMetadata();
    }
    std::vector<EmiChannelMeta> channels;
    channels.push_back(std::move(channel.value));
    return Meta::Success(std::move(channels));
}

/// @return The character in lower case (ASCII letters only; other bytes stay as they are).
char LowerChar(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }

/// @return The text in lower case.
std::string ToLower(const std::string& text) {
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), LowerChar);
    return lower;
}

/// Splits lower-case text into words at '_' and ' '. Digits do not split a word.
std::vector<std::string> SplitWords(const std::string& lower) {
    std::vector<std::string> words;
    std::size_t start = 0;
    while (start <= lower.size()) {
        std::size_t found = lower.find_first_of("_ ", start);
        std::size_t end = found == std::string::npos ? lower.size() : found;
        words.push_back(lower.substr(start, end - start));
        start = end + 1;
    }
    return words;
}

/// @return True when the lower-case word is one of the whole-computer words of SPEC 3.3.
bool IsSystemWord(const std::string& word) {
    return word == "psys" || word == "system" || word == "platform" || word == "total";
}

/// @return The part after "rapl_package<digits>_" of a lower-case name, or an empty string when the name has another shape.
std::string RaplSuffix(const std::string& lower) {
    const std::string prefix = "rapl_package";
    if (lower.compare(0, prefix.size(), prefix) != 0) {
        return std::string();
    }
    std::size_t digitsEnd = lower.find_first_not_of("0123456789", prefix.size());
    bool shapeOk = digitsEnd != std::string::npos && digitsEnd > prefix.size() && lower[digitsEnd] == '_';
    if (!shapeOk) {
        return std::string();
    }
    return lower.substr(digitsEnd + 1);
}

}  // namespace

Reading<std::vector<EmiChannelMeta>> ParseEmiMetadata(std::uint16_t version, std::span<const unsigned char> metadata) {
    if (version == 1) {
        return ParseVersion1(metadata);
    }
    if (version == 2) {
        return ParseVersion2(metadata);
    }
    return BadMetadata();
}

EmiKind ClassifyEmiChannel(const std::string& name) {
    std::string lower = ToLower(name);
    std::vector<std::string> words = SplitWords(lower);
    if (std::any_of(words.begin(), words.end(), IsSystemWord)) {
        return EmiKind::System;
    }
    std::string suffix = RaplSuffix(lower);
    if (suffix == "pkg" || lower == "cpu") {
        return EmiKind::CpuPackage;
    }
    if (suffix == "dram") {
        return EmiKind::Dram;
    }
    if (suffix == "pp0" || lower.compare(0, 4, "cpu_") == 0) {
        return EmiKind::CpuCores;
    }
    if (suffix == "pp1") {
        return EmiKind::Igpu;
    }
    return EmiKind::Other;
}

const char* EmiKindWord(EmiKind kind) {
    switch (kind) {
        case EmiKind::System: return "system";
        case EmiKind::CpuPackage: return "cpu_package";
        case EmiKind::Dram: return "dram";
        case EmiKind::CpuCores: return "cpu_cores";
        case EmiKind::Igpu: return "igpu";
        default: return "other";
    }
}

Reading<double> EmiPowerWatts(const EmiSample& previous, const EmiSample& current) {
    bool usable = previous.known && current.known;
    bool timeAdvanced = current.time100ns > previous.time100ns;
    bool energyForward = current.energyPicowattHours >= previous.energyPicowattHours;
    if (!usable || !timeAdvanced || !energyForward) {
        return Reading<double>::Failure(Status::Unavailable, 0);
    }
    double joules = static_cast<double>(current.energyPicowattHours - previous.energyPicowattHours) * kJoulesPerPicowattHour;
    double seconds = static_cast<double>(current.time100ns - previous.time100ns) * kSecondsPer100ns;
    return Reading<double>::Success(joules / seconds);
}

Reading<std::uint64_t> PicowattHoursToMicrojoules(std::uint64_t picowattHours) {
    // 1 pWh = 0.0036 uJ = 36 / 10000 uJ. Split the number so no step can overflow, then round to nearest.
    std::uint64_t whole = picowattHours / 10000;
    std::uint64_t rest = picowattHours % 10000;
    std::uint64_t restPart = (rest * 36 + 5000) / 10000;
    return Reading<std::uint64_t>::Success(whole * 36 + restPart);
}

}  // namespace ews
