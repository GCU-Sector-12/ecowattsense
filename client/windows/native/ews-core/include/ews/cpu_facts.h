// cpu_facts.h: pure helpers that turn raw processor answers into facts: the family, model and stepping from
// CPUID leaf 1, and the count of physical cores from the buffer of GetLogicalProcessorInformationEx.
// Used by machine_info.cpp (the real reader) and the tests. No Windows header is needed here.
#pragma once

#include <cstdint>
#include <optional>
#include <span>

namespace ews {

/// The processor signature of CPUID leaf 1, decoded.
struct CpuSignature {
    std::uint32_t family = 0;
    std::uint32_t model = 0;
    std::uint32_t stepping = 0;
};

/// Decodes EAX of CPUID leaf 1. Stepping is bits 3..0, base model 7..4, base family 11..8, extended model 19..16
/// and extended family 27..20. The family adds the extended family when the base family is 15. The model adds the
/// extended model (shifted by 4) when the base family is 6 or 15.
/// @param eax EAX of CPUID leaf 1.
/// @return The signature, or nothing when eax is 0 (the processor gave no signature).
std::optional<CpuSignature> DecodeCpuSignature(std::uint32_t eax);

/// Counts the records of a RelationProcessorCore answer of GetLogicalProcessorInformationEx. Each record starts with
/// Relationship (4 bytes) and Size (4 bytes); the records are variable in size and follow each other.
/// @param buffer The bytes the call filled.
/// @return The number of cores. Nothing when the buffer is empty, a record is shorter than its header, runs past
///         the buffer, a record is not a core record, or the count is 0 or above 4096.
std::optional<std::uint32_t> CountProcessorCores(std::span<const std::uint8_t> buffer);

}  // namespace ews
