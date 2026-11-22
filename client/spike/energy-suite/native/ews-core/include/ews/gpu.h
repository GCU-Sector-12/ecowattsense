// gpu.h: graphics adapters (DXGI) and, when nvml.dll can be loaded, the power and energy of NVIDIA cards (NVML).
// Used by ews-scan (sources "gpu" and "gpu_use") and the watch mode. The result is logged only; it is not used for energy (SPEC 3.3).
// The adapter LUID is kept in memory only, to match the instance names of the "GPU Engine" counters. It is never
// written to a file. No device path or driver version is read into the result.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ews/reading.h"

namespace ews {

/// The role of a hardware adapter on a computer with graphics switching (SPEC 3.5, source gpu).
enum class GpuRole {
    Only,             ///< The only hardware adapter.
    PowerSaving,      ///< The first hardware adapter for the minimum power preference.
    HighPerformance,  ///< The first hardware adapter for the high performance preference.
    Other             ///< Any other hardware adapter.
};

/// One graphics adapter.
struct GpuAdapter {
    std::string description;                  ///< UTF-8.
    std::uint32_t vendorId = 0;               ///< PCI vendor id (0x10DE NVIDIA, 0x8086 Intel, 0x1002 AMD).
    std::uint32_t deviceId = 0;               ///< PCI device id.
    std::uint64_t dedicatedMemoryMegabytes = 0;
    bool software = false;                    ///< True for a software adapter (for example the basic render driver).
    std::uint64_t luid = 0;                   ///< Adapter LUID as (high part << 32) | low part. In memory only.
    std::optional<GpuRole> role;              ///< Nothing for a software adapter or when the role is unknown.
};

/// The power of one NVIDIA device. `known` is false when NVML could not read that device.
struct NvmlPower {
    bool known = false;
    std::uint32_t milliwatts = 0;
};

/// What NVML says about one NVIDIA device, from one read.
struct NvmlDeviceFacts {
    std::string name;                           ///< UTF-8, empty when unknown.
    std::uint32_t vendorId = 0;                 ///< PCI vendor id, 0 when unknown.
    std::uint32_t deviceId = 0;                 ///< PCI device id, 0 when unknown.
    bool powerAnswered = false;                 ///< The power read answered.
    bool energyAnswered = false;                ///< The total energy counter answered.
    std::optional<std::uint32_t> milliwatts;    ///< The power read, when it answered.
};

/// One reading of one NVIDIA device. An absent value means the read did not answer.
struct NvmlSample {
    std::optional<std::uint32_t> milliwatts;    ///< Power in mW.
    std::optional<std::uint64_t> millijoules;   ///< Total energy counter in mJ since the driver was loaded.
};

/// An open NVML session that stays loaded between reads (the watch mode reads every period). Tests use a fake.
class INvmlSession {
public:
    virtual ~INvmlSession() = default;
    /// Loads NVML, starts it, lists the devices and reads each one once.
    /// @return Ok with one entry per device. Unavailable when nvml.dll or a needed function is missing.
    ///         Error when NVML fails to start or to count the devices.
    virtual Reading<std::vector<NvmlDeviceFacts>> Open() = 0;
    /// Reads power and energy of every device found by Open.
    /// @return Ok with one sample per device, in the order of Open. Unavailable when Open did not succeed.
    virtual Reading<std::vector<NvmlSample>> Read() = 0;
};

/// A source of graphics information. Tests use a fake.
class IGpuSource {
public:
    virtual ~IGpuSource() = default;
    /// Lists the adapters, with the role of each hardware adapter when the factory can tell.
    /// @return Ok with the adapters (an empty list when DXGI knows none), or the failure from DXGI.
    virtual Reading<std::vector<GpuAdapter>> ListAdapters() = 0;
    /// Reads the power of every NVIDIA device through NVML.
    /// @return Ok with one entry per device. Unavailable when nvml.dll or one of its functions is missing.
    ///         Error when NVML fails to start or to count the devices.
    virtual Reading<std::vector<NvmlPower>> ReadNvmlPower() = 0;
    /// Reads name, PCI ids, power and energy of every NVIDIA device through NVML, once.
    /// @return As INvmlSession::Open.
    virtual Reading<std::vector<NvmlDeviceFacts>> ReadNvmlDevices() = 0;
};

/// The real source: DXGI for the list, a dynamic load of nvml.dll (System32 only) for the power.
class SystemGpuSource final : public IGpuSource {
public:
    Reading<std::vector<GpuAdapter>> ListAdapters() override;
    Reading<std::vector<NvmlPower>> ReadNvmlPower() override;
    Reading<std::vector<NvmlDeviceFacts>> ReadNvmlDevices() override;
};

/// The real NVML session (nvml_session.cpp). It frees the library and shuts NVML down in the destructor.
class SystemNvmlSession final : public INvmlSession {
public:
    SystemNvmlSession();
    ~SystemNvmlSession() override;
    SystemNvmlSession(const SystemNvmlSession&) = delete;
    SystemNvmlSession& operator=(const SystemNvmlSession&) = delete;

    Reading<std::vector<NvmlDeviceFacts>> Open() override;
    Reading<std::vector<NvmlSample>> Read() override;

    /// The state of the source; its layout is private to the .cpp file.
    struct State;

private:
    std::unique_ptr<State> state_;
};

/// @param role A role.
/// @return The fixed word: only, power_saving, high_performance or other.
const char* GpuRoleWord(GpuRole role);

}  // namespace ews
