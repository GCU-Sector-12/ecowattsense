// cpu_facts.cpp: the pure processor helpers (see ews/cpu_facts.h).
// Used by machine_info.cpp and the tests. The buffer comes from Windows and is bounds-checked before every read.

#include "ews/cpu_facts.h"

namespace ews {

namespace {

constexpr std::size_t kRecordHeaderBytes = 8;       // Relationship (4) + Size (4)
constexpr std::uint32_t kRelationProcessorCore = 0;
constexpr std::uint32_t kMaxCores = 4096;           // a sane upper bound for one computer
constexpr std::uint32_t kFamilyExtended = 0xF;
constexpr std::uint32_t kFamilySix = 6;

/// Reads a 32-bit little-endian value. The caller has checked that four bytes exist at the offset.
std::uint32_t ReadU32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

/// Reads the core record at `offset`.
/// @return The size of the record, or 0 when the header does not fit, the size is below the header, the record runs past
///         the buffer, or the record is not a core record.
std::size_t CoreRecordSize(std::span<const std::uint8_t> bytes, std::size_t offset) {
    if (bytes.size() - offset < kRecordHeaderBytes) {
        return 0;
    }
    std::size_t size = ReadU32(bytes, offset + 4);
    bool fits = size >= kRecordHeaderBytes && size <= bytes.size() - offset;
    bool isCore = ReadU32(bytes, offset) == kRelationProcessorCore;
    return fits && isCore ? size : 0;
}

}  // namespace

std::optional<CpuSignature> DecodeCpuSignature(std::uint32_t eax) {
    if (eax == 0) {
        return std::nullopt;
    }
    std::uint32_t baseFamily = (eax >> 8) & 0xF;
    std::uint32_t baseModel = (eax >> 4) & 0xF;
    std::uint32_t extendedModel = (eax >> 16) & 0xF;
    std::uint32_t extendedFamily = (eax >> 20) & 0xFF;
    CpuSignature signature;
    signature.stepping = eax & 0xF;
    signature.family = baseFamily == kFamilyExtended ? baseFamily + extendedFamily : baseFamily;
    bool usesExtendedModel = baseFamily == kFamilyExtended || baseFamily == kFamilySix;
    signature.model = usesExtendedModel ? (extendedModel << 4) + baseModel : baseModel;
    return signature;
}

std::optional<std::uint32_t> CountProcessorCores(std::span<const std::uint8_t> buffer) {
    std::size_t offset = 0;
    std::uint32_t cores = 0;
    bool good = true;
    while (good && offset < buffer.size()) {
        std::size_t size = CoreRecordSize(buffer, offset);
        good = size != 0;
        offset += size;
        cores += good ? 1u : 0u;
    }
    bool believable = good && cores > 0 && cores <= kMaxCores;
    return believable ? std::optional<std::uint32_t>(cores) : std::nullopt;
}

}  // namespace ews
