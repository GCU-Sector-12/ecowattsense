// watch_session.h: the watch mode of ews-scan (SPEC 1.6) without the clock and the output: it reads the sources
// and gives the text of the header line and of each period line.
// Used by ews-scan (the loop with the timer) and the tests (with fake sources).
// A failed read is "unknown": a null value, never 0 and never "does not exist".
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ews/battery.h"
#include "ews/display.h"
#include "ews/display_layout.h"
#include "ews/emi.h"
#include "ews/gpu.h"
#include "ews/gpu_link.h"
#include "ews/gpu_use.h"
#include "ews/machine_info.h"
#include "ews/watch_engine.h"

namespace ews {

/// The sources the watch reads. The caller owns them; they must outlive the session.
struct WatchSources {
    IMachineInfoSource& machine;
    IEmiSource& emi;
    IBatterySource& battery;
    IDisplaySource& display;
    IDisplayLayoutSource& displayLayout;
    IGpuSource& gpu;
    INvmlSession& nvml;
    IGpuUseSource& gpuUse;
    Reading<bool> (*onBattery)();  ///< The on-battery reading (ReadOnBattery in the real program).
};

/// One watch run.
class WatchSession {
public:
    /// @param sources The sources.
    /// @param periodMs The period in the header (500..10000, checked by the argument parser).
    WatchSession(const WatchSources& sources, std::uint32_t periodMs);

    /// Reads the static facts, opens the EMI and NVML devices, lists the adapters and primes the GPU Engine query.
    /// @return The header line. A source that fails gives an empty list in the header, never a stop.
    std::string Start();

    /// Reads every source once and builds the line of this period. Call it after Start.
    /// @param tMs Milliseconds on a monotonic clock since the watch started.
    /// @return One period line.
    std::string Sample(std::int64_t tMs);

private:
    WatchSources sources_;
    std::uint32_t periodMs_;
    std::vector<EmiChannel> channels_;
    std::vector<NvmlDeviceFacts> nvmlDevices_;
    bool nvmlOpen_ = false;
    std::vector<GpuUseTarget> targets_;
    WatchEngine engine_;
};

}  // namespace ews
