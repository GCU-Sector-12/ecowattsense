// gpu_link.cpp: the pure graphics decisions (see ews/gpu_link.h): roles, the NVML to adapter match, the watch targets.
// Used by gpu.cpp, the scanner and the watch session. No Windows header is needed here.

#include "ews/gpu_link.h"

#include <algorithm>

namespace ews {

namespace {

constexpr std::uint32_t kNvidiaVendor = 0x10DE;

/// @return True when the adapter is a hardware adapter.
bool IsHardware(const GpuAdapter& adapter) { return !adapter.software; }

/// A predicate: the adapter is a hardware adapter with this LUID.
struct HardwareWithLuid {
    std::uint64_t luid;
    bool operator()(const GpuAdapter& adapter) const { return IsHardware(adapter) && adapter.luid == luid; }
};

/// @return The index of the hardware adapter with this LUID, or nothing.
std::optional<std::size_t> HardwareIndexOfLuid(const std::vector<GpuAdapter>& adapters, std::uint64_t luid) {
    auto found = std::find_if(adapters.begin(), adapters.end(), HardwareWithLuid{luid});
    if (found == adapters.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - adapters.begin());
}

/// Sets the role of one adapter when it is a hardware adapter.
void SetRoleIfHardware(GpuAdapter& adapter, GpuRole role) {
    if (IsHardware(adapter)) {
        adapter.role = role;
    }
}

/// Gives every hardware adapter the same role.
void SetRoleOfHardware(std::vector<GpuAdapter>& adapters, GpuRole role) {
    for (GpuAdapter& adapter : adapters) {
        SetRoleIfHardware(adapter, role);
    }
}

/// Sets the roles of a computer with more than one hardware adapter.
void AssignRolesOfSeveral(std::vector<GpuAdapter>& adapters, const GpuPreferenceChoice& choice) {
    SetRoleOfHardware(adapters, GpuRole::Other);
    auto powerSaving = HardwareIndexOfLuid(adapters, choice.powerSavingLuid);
    auto highPerformance = HardwareIndexOfLuid(adapters, choice.highPerformanceLuid);
    bool different = powerSaving.has_value() && highPerformance.has_value() && *powerSaving != *highPerformance;
    if (!different) {
        return;
    }
    adapters[*powerSaving].role = GpuRole::PowerSaving;
    adapters[*highPerformance].role = GpuRole::HighPerformance;
}

/// @return The first position below `count` where the predicate is true, or `count` when there is none.
template <typename Predicate>
std::size_t FirstPositionWhere(std::size_t count, Predicate predicate) {
    std::size_t position = 0;
    while (position < count && !predicate(position)) {
        ++position;
    }
    return position;
}

/// A predicate over adapter positions: the adapter is free and has the PCI ids of the device.
struct FreeMatch {
    const std::vector<GpuAdapter>& adapters;
    const std::vector<bool>& taken;
    const NvmlDeviceFacts& device;
    bool operator()(std::size_t position) const {
        const GpuAdapter& adapter = adapters[position];
        return !taken[position] && adapter.vendorId == device.vendorId && adapter.deviceId == device.deviceId;
    }
};

/// @return True when the device ids are known (NVML gave them).
bool IdsKnown(const NvmlDeviceFacts& device) { return device.vendorId != 0 || device.deviceId != 0; }

/// Finds the adapter of one device and marks it as taken.
/// @return The index of the first free adapter with the same ids, or nothing.
std::optional<std::size_t> LinkOne(const NvmlDeviceFacts& device, const std::vector<GpuAdapter>& adapters, std::vector<bool>& taken) {
    if (!IdsKnown(device)) {
        return std::nullopt;
    }
    std::size_t position = FirstPositionWhere(adapters.size(), FreeMatch{adapters, taken, device});
    if (position == adapters.size()) {
        return std::nullopt;
    }
    taken[position] = true;
    return position;
}

/// Adds the target of one adapter to the list when it is a hardware adapter.
void AppendTarget(std::vector<GpuUseTarget>& targets, const GpuAdapter& adapter, std::size_t position) {
    if (!IsHardware(adapter)) {
        return;
    }
    GpuUseTarget target;
    target.adapter = position;
    target.luid = adapter.luid;
    target.role = adapter.role;
    target.vendorId = adapter.vendorId;
    targets.push_back(target);
}

}  // namespace

void AssignGpuRoles(std::vector<GpuAdapter>& adapters, const std::optional<GpuPreferenceChoice>& choice) {
    for (GpuAdapter& adapter : adapters) {
        adapter.role.reset();
    }
    std::ptrdiff_t hardware = std::count_if(adapters.begin(), adapters.end(), IsHardware);
    if (!choice.has_value() || hardware == 0) {
        return;
    }
    if (hardware == 1) {
        SetRoleOfHardware(adapters, GpuRole::Only);
        return;
    }
    AssignRolesOfSeveral(adapters, *choice);
}

bool SplitNvmlPciId(std::uint32_t combined, std::uint32_t& vendorId, std::uint32_t& deviceId) {
    std::uint32_t vendor = combined & 0xFFFF;
    if (vendor != kNvidiaVendor) {
        return false;
    }
    vendorId = vendor;
    deviceId = combined >> 16;
    return true;
}

std::vector<std::optional<std::size_t>> LinkNvmlToAdapters(const std::vector<NvmlDeviceFacts>& devices,
                                                           const std::vector<GpuAdapter>& adapters) {
    std::vector<bool> taken(adapters.size(), false);
    std::vector<std::optional<std::size_t>> links;
    for (const NvmlDeviceFacts& device : devices) {
        links.push_back(LinkOne(device, adapters, taken));
    }
    return links;
}

std::vector<GpuUseTarget> HardwareAdapterTargets(const std::vector<GpuAdapter>& adapters) {
    std::vector<GpuUseTarget> targets;
    for (std::size_t i = 0; i < adapters.size(); ++i) {
        AppendTarget(targets, adapters[i], i);
    }
    return targets;
}

const char* GpuRoleWord(GpuRole role) {
    switch (role) {
        case GpuRole::Only: return "only";
        case GpuRole::PowerSaving: return "power_saving";
        case GpuRole::HighPerformance: return "high_performance";
        case GpuRole::Other: return "other";
    }
    return "other";
}

}  // namespace ews
