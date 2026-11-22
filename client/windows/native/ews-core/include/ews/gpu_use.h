// gpu_use.h: the use of each graphics adapter from the instances of the performance counter set "GPU Engine",
// counter "Utilization Percentage". Pure rules (instance names, grouping, sums) plus the real PDH source.
// Used by ews-scan (source "gpu_use") and the watch mode. The rule per adapter is the one of the calibrator
// (LoadReaders.cs, Busiest3DEngine): 3D instances, grouped by engine, summed over processes, highest engine, 0..100.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ews/pdh_instance.h"
#include "ews/reading.h"

namespace ews {

/// The use of one adapter, found by its LUID.
struct AdapterUse {
    std::uint64_t luid = 0;
    std::optional<double> percent;  ///< Nothing when no 3D instance of this adapter was read.
};

/// The result of grouping all instances of one read.
struct GpuUseSummary {
    std::vector<AdapterUse> adapters;  ///< One entry per different adapter id seen in the instance names.
};

/// Reads the adapter id from an instance name like "pid_12_luid_0x00000000_0x0000A1B2_phys_0_eng_0_engtype_3D".
/// @param name The instance name (untrusted).
/// @return (high part << 32) | low part, or nothing when the name has no well formed "luid_0x<hex>_0x<hex>".
std::optional<std::uint64_t> ParseInstanceLuid(const std::string& name);

/// Groups the instances of one read per adapter (SPEC 1.5). Instances with a value that is not a finite number
/// at or above 0 count as not read. Names without an adapter id are ignored.
/// @param instances The instances and values.
/// @return One entry per adapter id seen in any instance name, in order of first appearance.
GpuUseSummary SummariseGpuUse(const std::vector<PdhInstance>& instances);

/// @param summary A summary.
/// @param luid The adapter LUID.
/// @return The use of that adapter in percent (0..100), or nothing when it is unknown.
std::optional<double> GpuUseOfAdapter(const GpuUseSummary& summary, std::uint64_t luid);

/// A source of GPU Engine instances. Tests use a fake.
class IGpuUseSource {
public:
    virtual ~IGpuUseSource() = default;
    /// Collects the counters once and reads every instance. The first call only primes the query: a rate counter needs
    /// two collections, so it gives Unavailable.
    /// @return Ok with the instances that had a valid value. Unavailable when the counter set does not exist or has no
    ///         value yet. Error for any other PDH failure.
    virtual Reading<std::vector<PdhInstance>> Read() = 0;
};

/// The real source: one PDH query kept between calls (PdhAddEnglishCounterW, wildcard instances).
class SystemGpuUseSource final : public IGpuUseSource {
public:
    SystemGpuUseSource();
    ~SystemGpuUseSource() override;
    SystemGpuUseSource(const SystemGpuUseSource&) = delete;
    SystemGpuUseSource& operator=(const SystemGpuUseSource&) = delete;

    Reading<std::vector<PdhInstance>> Read() override;

    /// The state of the source; its layout is private to the .cpp file.
    struct State;

private:
    std::unique_ptr<State> state_;
};

}  // namespace ews
