// cpu_times.h: system-wide processor times and the processor load computed from two samples.
// Used by ews-scan (the "live" step of a source test) and ews-service (the load of each interval).
#pragma once

#include <cstdint>

#include "ews/reading.h"

namespace ews {

/// Cumulative processor times since boot, in 100 ns ticks (as GetSystemTimes gives them).
/// Kernel time INCLUDES idle time.
struct CpuTimes {
    std::uint64_t idle = 0;
    std::uint64_t kernel = 0;
    std::uint64_t user = 0;
};

/// A source of processor times. Tests use a fake.
class ICpuTimesSource {
public:
    virtual ~ICpuTimesSource() = default;
    /// Reads the cumulative times now.
    /// @return Ok with the times, or the failure from Windows.
    virtual Reading<CpuTimes> Read() = 0;
};

/// The real source, backed by GetSystemTimes.
class SystemCpuTimesSource final : public ICpuTimesSource {
public:
    /// Reads the times with GetSystemTimes. A failed call gives the status for GetLastError().
    Reading<CpuTimes> Read() override;
};

/// Processor load between two samples, 0 to 100 percent.
/// Busy time is (kernel + user) minus idle, because kernel includes idle.
/// @param previous The earlier sample (the first read has none: the caller must not call this then).
/// @param current The later sample.
/// @return Ok with the percent. Unavailable when a counter went backwards, the total delta is zero,
///         or the idle delta is larger than the total delta (inconsistent counters).
Reading<double> CpuPercent(const CpuTimes& previous, const CpuTimes& current);

}  // namespace ews
