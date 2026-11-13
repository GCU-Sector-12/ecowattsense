// machine_info.h: maker, model, chassis, processor and Windows build of this computer.
// Used by ews-scan (source "machine") and ews-service (device type: laptop or desktop).
// The SMBIOS parser is a pure function over bytes. It never reads the serial number or UUID fields.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "ews/reading.h"

namespace ews {

/// What the SMBIOS table says. An absent value means "unknown".
struct SmbiosInfo {
    std::optional<std::string> maker;          ///< Type 1, Manufacturer.
    std::optional<std::string> model;          ///< Type 1, Product Name.
    std::optional<std::uint8_t> chassisType;   ///< Type 3, chassis type byte with bit 7 (lock flag) removed.
};

/// The machine fields. An absent value means "unknown".
struct MachineInfo {
    SmbiosInfo smbios;
    std::optional<std::string> processorVendor;   ///< CPUID leaf 0, e.g. "GenuineIntel".
    std::optional<std::string> processorBrand;    ///< CPUID brand string, if the processor has one.
    std::optional<std::uint32_t> logicalProcessors;
    std::optional<std::uint32_t> windowsBuild;
};

/// A source of machine facts. Tests use a fake.
class IMachineInfoSource {
public:
    virtual ~IMachineInfoSource() = default;
    /// Reads the facts. A part that cannot be read is left unknown; the call itself still gives Ok.
    virtual Reading<MachineInfo> Read() = 0;
};

/// The real source (SMBIOS through GetSystemFirmwareTable, CPUID, GetActiveProcessorCount, RtlGetVersion).
class SystemMachineInfoSource final : public IMachineInfoSource {
public:
    Reading<MachineInfo> Read() override;
};

/// Cuts the structure table out of the raw 'RSMB' firmware table (which starts with an 8-byte header).
/// @param raw The bytes from GetSystemFirmwareTable('RSMB').
/// @return The structure table; empty when raw is shorter than the header. A length field larger
///         than the buffer is clamped to the buffer.
std::span<const std::uint8_t> ExtractSmbiosTable(std::span<const std::uint8_t> raw);

/// Parses the SMBIOS structure table for type 1 (maker, model) and type 3 (chassis type).
/// Survives a truncated table, a structure length below 4, a string index beyond the string set,
/// a missing string-set terminator, and a missing type 1 or 3 (that field stays unknown).
/// @param table The structure table bytes (without the 8-byte header).
/// @return The fields that were found.
SmbiosInfo ParseSmbios(std::span<const std::uint8_t> table);

/// Maps an SMBIOS chassis type to a word.
/// @param chassisType The type byte; bit 7 is ignored.
/// @return "laptop" for 8, 9, 10, 11, 14, 30, 31, 32; "desktop" for 3, 4, 5, 6, 7, 13, 35, 36; else "other".
const char* ChassisWord(std::uint8_t chassisType);

}  // namespace ews
