// gpu.h: graphics adapters (DXGI) and, when nvml.dll can be loaded, the power of NVIDIA cards (NVML).
// Used by ews-scan (source "gpu"). The result is logged only; it is not used for energy (SPEC 3.3).
// No adapter LUID, device path or driver version is read into the result.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ews/reading.h"

namespace ews {

/// One graphics adapter.
struct GpuAdapter {
    std::string description;                  ///< UTF-8.
    std::uint32_t vendorId = 0;               ///< PCI vendor id (0x10DE NVIDIA, 0x8086 Intel, 0x1002 AMD).
    std::uint64_t dedicatedMemoryMegabytes = 0;
    bool software = false;                    ///< True for a software adapter (for example the basic render driver).
};

/// The power of one NVIDIA device. `known` is false when NVML could not read that device.
struct NvmlPower {
    bool known = false;
    std::uint32_t milliwatts = 0;
};

/// A source of graphics information. Tests use a fake.
class IGpuSource {
public:
    virtual ~IGpuSource() = default;
    /// Lists the adapters.
    /// @return Ok with the adapters (an empty list when DXGI knows none), or the failure from DXGI.
    virtual Reading<std::vector<GpuAdapter>> ListAdapters() = 0;
    /// Reads the power of every NVIDIA device through NVML.
    /// @return Ok with one entry per device. Unavailable when nvml.dll or one of its functions is missing.
    ///         Error when NVML fails to start or to count the devices.
    virtual Reading<std::vector<NvmlPower>> ReadNvmlPower() = 0;
};

/// The real source: DXGI for the list, a dynamic load of nvml.dll (System32 only) for the power.
class SystemGpuSource final : public IGpuSource {
public:
    Reading<std::vector<GpuAdapter>> ListAdapters() override;
    Reading<std::vector<NvmlPower>> ReadNvmlPower() override;
};

}  // namespace ews
