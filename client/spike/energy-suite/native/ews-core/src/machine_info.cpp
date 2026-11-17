// machine_info.cpp: the real machine facts source (see ews/machine_info.h).
// Used by ews-scan and ews-service. SMBIOS parsing itself lives in smbios.cpp.
// Every part is read on its own; a part that fails stays unknown and never stops the others.

#include <windows.h>
#include <intrin.h>

#include <array>
#include <cstring>
#include <string>
#include <vector>

#include "ews/machine_info.h"

namespace ews {

namespace {

constexpr DWORD kSmbiosSignature = 'RSMB';       // firmware table provider
constexpr UINT kMaxSmbiosBytes = 4u * 1024 * 1024;  // a larger table is not believable
constexpr unsigned kLeafMaxExtended = 0x80000000u;
constexpr unsigned kLeafBrandFirst = 0x80000002u;
constexpr unsigned kLeafBrandLast = 0x80000004u;

/// Reads the raw SMBIOS table.
/// @return The bytes; empty when the call fails or returns an unbelievable size (the table is then unknown).
std::vector<std::uint8_t> ReadRawSmbios() {
    UINT size = GetSystemFirmwareTable(kSmbiosSignature, 0, nullptr, 0);
    if (size == 0 || size > kMaxSmbiosBytes) {
        return {};
    }
    std::vector<std::uint8_t> buffer(size);
    UINT got = GetSystemFirmwareTable(kSmbiosSignature, 0, buffer.data(), size);
    if (got == 0 || got > size) {
        return {};
    }
    buffer.resize(got);
    return buffer;
}

/// Executes CPUID.
/// @param leaf The CPUID leaf.
/// @return EAX, EBX, ECX, EDX in this order.
std::array<int, 4> Cpuid(unsigned leaf) {
    std::array<int, 4> regs{};
    __cpuid(regs.data(), static_cast<int>(leaf));
    return regs;
}

/// Reads the 12-character vendor string of the processor (leaf 0: EBX, EDX, ECX).
/// @return The vendor text.
std::optional<std::string> ReadVendor() {
    std::array<int, 4> regs = Cpuid(0);
    char text[13] = {};
    std::memcpy(text, &regs[1], 4);
    std::memcpy(text + 4, &regs[3], 4);
    std::memcpy(text + 8, &regs[2], 4);
    return std::string(text, 12);
}

/// Reads the 48-byte brand string (leaves 0x80000002 to 0x80000004).
/// @return The brand, or nothing when the processor does not support these leaves or the text is empty.
std::optional<std::string> ReadBrand() {
    unsigned maxLeaf = static_cast<unsigned>(Cpuid(kLeafMaxExtended)[0]);
    if (maxLeaf < kLeafBrandLast) {
        return std::nullopt;
    }
    char text[49] = {};
    for (unsigned leaf = kLeafBrandFirst; leaf <= kLeafBrandLast; ++leaf) {
        std::array<int, 4> regs = Cpuid(leaf);
        std::memcpy(text + (leaf - kLeafBrandFirst) * 16, regs.data(), 16);
    }
    std::string brand(text);
    std::size_t first = brand.find_first_not_of(' ');
    if (first == std::string::npos) {
        return std::nullopt;
    }
    return brand.substr(first);
}

/// Reads the Windows build number with RtlGetVersion (GetVersionEx hides the real number without a manifest).
/// @return The build number, or nothing when ntdll or the function cannot be found.
std::optional<std::uint32_t> ReadWindowsBuild() {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr) {
        return std::nullopt;
    }
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    auto function = reinterpret_cast<RtlGetVersionFn>(reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")));
    if (function == nullptr) {
        return std::nullopt;
    }
    OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    if (function(&version) != 0) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(version.dwBuildNumber);
}

/// Counts the logical processors of all processor groups.
/// @return The count, or nothing when Windows returns 0.
std::optional<std::uint32_t> ReadLogicalProcessors() {
    DWORD count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (count == 0) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(count);
}

}  // namespace

Reading<MachineInfo> SystemMachineInfoSource::Read() {
    MachineInfo info;
    std::vector<std::uint8_t> raw = ReadRawSmbios();
    info.smbios = ParseSmbios(ExtractSmbiosTable(raw));
    info.processorVendor = ReadVendor();
    info.processorBrand = ReadBrand();
    info.logicalProcessors = ReadLogicalProcessors();
    info.windowsBuild = ReadWindowsBuild();
    return Reading<MachineInfo>::Success(std::move(info));
}

}  // namespace ews
