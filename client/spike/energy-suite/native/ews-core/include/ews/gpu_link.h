// gpu_link.h: pure decisions about graphics adapters: the role of each hardware adapter, the match of NVML devices
// to adapters, the PCI ids inside the NVML combined id, and the list of adapters the watch mode reads.
// Used by gpu.cpp (the real source), the scanner and the watch session. No Windows header is needed here.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "ews/gpu.h"

namespace ews {

/// The LUIDs of the first hardware adapter that the factory gives for each GPU preference.
struct GpuPreferenceChoice {
    std::uint64_t powerSavingLuid = 0;      ///< For DXGI_GPU_PREFERENCE_MINIMUM_POWER.
    std::uint64_t highPerformanceLuid = 0;  ///< For DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE.
};

/// Sets the role of each adapter (SPEC 3.5). A software adapter gets none. Without a preference answer
/// (the factory interface is missing, or a call failed) no adapter gets a role. With one hardware adapter it is "only".
/// With more: the adapter of each preference gets power_saving or high_performance when the two differ, and every
/// other hardware adapter gets "other". When the two choices are the same adapter, or one is not in the list,
/// every hardware adapter gets "other".
/// @param adapters The adapters; their role fields are set.
/// @param choice What the factory said, or nothing.
void AssignGpuRoles(std::vector<GpuAdapter>& adapters, const std::optional<GpuPreferenceChoice>& choice);

/// Splits the combined PCI id of NVML (16-bit device id in the high half, 16-bit vendor id in the low half).
/// @param combined The value of pciDeviceId.
/// @param vendorId Receives the vendor id.
/// @param deviceId Receives the device id.
/// @return True only when the vendor is NVIDIA (0x10DE), the only vendor NVML serves. Otherwise the layout is not
///         trusted and nothing is set.
bool SplitNvmlPciId(std::uint32_t combined, std::uint32_t& vendorId, std::uint32_t& deviceId);

/// Matches NVML devices to adapters: for each device, the first adapter (not yet taken) with the same PCI vendor
/// id and device id. A device with unknown ids, or with no adapter left, has no match.
/// @param devices The NVML devices.
/// @param adapters The adapters of the gpu source.
/// @return One entry per device: the index in `adapters`, or nothing.
std::vector<std::optional<std::size_t>> LinkNvmlToAdapters(const std::vector<NvmlDeviceFacts>& devices,
                                                           const std::vector<GpuAdapter>& adapters);

/// One hardware adapter that the watch mode reads the GPU use of.
struct GpuUseTarget {
    std::size_t adapter = 0;            ///< Index in the adapter list of the gpu source.
    std::uint64_t luid = 0;
    std::optional<GpuRole> role;
    std::uint32_t vendorId = 0;
};

/// Lists the hardware adapters in the order of the adapter list.
/// @param adapters The adapters of the gpu source.
/// @return One target per adapter that is not a software adapter.
std::vector<GpuUseTarget> HardwareAdapterTargets(const std::vector<GpuAdapter>& adapters);

}  // namespace ews
