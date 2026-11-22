// scan.h: the scan of all sources (SPEC 3): the result types, the pure decision functions and RunScan.
// Used by ews-scan (writes capabilities.json and scan.log) and, later, by ews-service (re-scan in the service context).
// The result never holds a serial number, user name, computer name or device path.
// A failed step is "unknown", never "does not exist". The scan never stops on one failed source.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ews/battery.h"
#include "ews/display.h"
#include "ews/display_layout.h"
#include "ews/emi.h"
#include "ews/gpu.h"
#include "ews/gpu_use.h"
#include "ews/intel_dtt.h"
#include "ews/machine_info.h"
#include "ews/pawnio.h"
#include "ews/power_meter.h"
#include "ews/reading.h"

namespace ews {

/// A yes / no / unknown answer. Unknown is never shown as false.
enum class Tri { False, True, Unknown };

/// Where the scan runs (the value of --context).
enum class ScanContext { User, Admin, Service };

/// The measurement level of SPEC 3.4.
enum class MeasurementLevel { L1, L2, L3, L4 };

/// The calibration word of SPEC 3.4.
enum class Calibration { NotNeeded, Needed, NotPossible };

/// The device type of SPEC 3.4.
enum class DeviceType { Laptop, Desktop };

/// The state of NVML after the graphics test.
enum class NvmlState { Ok, Unavailable, Error };

/// The four common steps of SPEC 3.2 for one source, plus the sample for the log.
struct SourceCommon {
    Tri present = Tri::Unknown;
    Tri readable = Tri::Unknown;
    Tri live = Tri::Unknown;
    unsigned long error = 0;       ///< Windows error code of the first failure, 0 when there was none.
    std::optional<double> sample;  ///< One value in the unit of the source; absent when unknown.
};

/// One EMI channel after the live test.
struct EmiChannelResult {
    std::uint32_t device = 0;
    std::uint32_t index = 0;
    std::string name;
    EmiKind kind = EmiKind::Other;
    Tri live = Tri::Unknown;
    std::optional<double> sampleWatts;
};

/// The EMI source: common steps (sample = largest channel power) and every channel.
struct EmiResult {
    SourceCommon common;
    std::vector<EmiChannelResult> channels;
};

/// The battery source, summed over all batteries.
struct BatteryResult {
    SourceCommon common;                   ///< sample = the summed full charged capacity in mWh, when known.
    std::optional<std::uint32_t> count;
    std::optional<BatteryUnit> unit;       ///< MilliwattHours when at least one battery reports mWh.
    std::optional<bool> rateKnown;         ///< True when at least one battery reports its rate.
    std::optional<std::uint32_t> fullMwh;  ///< Sum of the known full charged capacities of the mWh batteries.
    std::optional<std::uint32_t> designMwh;   ///< Sum of the known designed capacities of the mWh batteries.
    std::optional<std::uint32_t> cycleCount;  ///< The largest known cycle count; 0 (unknown) is left out.
    bool reportsMwh = false;               ///< At least one battery has unit mWh and a known full charged capacity.
};

/// The ACPI power meter.
struct PowerMeterResult {
    SourceCommon common;  ///< sample = the largest value read, in the unit of the counter.
    std::optional<std::uint32_t> instances;
};

/// The display brightness and layout.
struct DisplayResult {
    SourceCommon common;  ///< sample = brightness in percent, when known.
    std::optional<int> brightnessPercent;
    std::optional<std::uint32_t> displays;          ///< Active displays.
    std::optional<std::uint32_t> externalDisplays;  ///< Active displays that are not the built-in panel.
    std::optional<std::uint32_t> refreshHz;         ///< Of the built-in panel, or of the first display.
};

/// One NVIDIA device of the list "nvml" of source gpu.
struct NvmlEntry {
    std::string name;
    std::optional<std::size_t> adapter;  ///< Index in `adapters`; nothing when no adapter has the same PCI ids.
    bool power = false;                  ///< The power read answered.
    bool energy = false;                 ///< The total energy counter answered.
    std::optional<double> sampleWatts;   ///< The power read in watts.
};

/// The graphics adapters and NVML.
struct GpuResult {
    SourceCommon common;  ///< sample = the largest NVML power in watts, when known.
    std::vector<GpuAdapter> adapters;
    std::optional<NvmlState> nvml;
    std::optional<std::uint32_t> nvmlDevices;
    std::vector<NvmlEntry> nvmlEntries;  ///< One entry per NVML device, in the order NVML lists them.
};

/// The use of the graphics adapters: the counter set "GPU Engine".
struct GpuUseResult {
    SourceCommon common;  ///< sample = the highest use of one adapter in percent, when known. live stays unknown.
    std::optional<std::uint32_t> adapters;  ///< Count of different adapter ids seen in the instance names.
};

/// Intel DTT: only the common steps.
struct IntelDttResult {
    SourceCommon common;
};

/// PawnIO: the common steps and the version, when it is known and valid.
struct PawnIoResult {
    SourceCommon common;
    std::optional<std::string> version;  ///< Only a valid version text (1 to 32 digits and dots).
};

/// The whole scan: everything that goes into capabilities.json.
struct ScanResult {
    std::string scannedUtc;  ///< "2026-11-14T10:00:00Z".
    ScanContext context = ScanContext::User;
    MachineInfo machine;
    EmiResult emi;
    BatteryResult battery;
    PowerMeterResult powerMeter;
    DisplayResult display;
    GpuResult gpu;
    GpuUseResult gpuUse;
    IntelDttResult intelDtt;
    PawnIoResult pawnIo;
    MeasurementLevel level = MeasurementLevel::L4;
    Calibration calibration = Calibration::NotPossible;
    DeviceType deviceType = DeviceType::Desktop;
};

/// The facts the level rule needs (SPEC 3.4).
struct LevelFacts {
    bool liveSystemChannel = false;  ///< A live EMI channel of kind system.
    bool liveCpuPackage = false;     ///< A live EMI channel of kind cpu_package.
    bool mwhBattery = false;         ///< A battery with unit mWh and a known full charged capacity.
};

/// Decides the level. L1: live system channel (the ACPI power meter never counts, SPEC 12.6).
/// L2: live cpu_package channel.
/// L3: mWh battery. L4: nothing of that. The rules are tested in this order.
/// @param facts What the scan found.
/// @return The level.
MeasurementLevel DecideLevel(const LevelFacts& facts);

/// Decides the calibration word. L1 gives not_needed; L2 and L3 give needed with an mWh battery,
/// else not_possible; L4 gives not_possible.
/// @param level The level from DecideLevel.
/// @param mwhBattery True when a battery reports mWh with a known full charged capacity.
/// @return The calibration word.
Calibration DecideCalibration(MeasurementLevel level, bool mwhBattery);

/// Decides the device type: laptop when the chassis type is a portable one or a battery is present, else desktop.
/// @param chassisType The SMBIOS chassis type byte, or nothing when unknown.
/// @param batteryPresent True when the battery source says a battery exists.
/// @return The device type.
DeviceType DecideDeviceType(std::optional<std::uint8_t> chassisType, bool batteryPresent);

/// @param value A Tri.
/// @return The fixed word for the log and the console: "yes", "no" or "unknown".
const char* TriWord(Tri value);
/// @param context A context.
/// @return The context word: user, admin or service.
const char* ContextWord(ScanContext context);
/// @param level A level.
/// @return The level word: L1, L2, L3 or L4.
const char* LevelWord(MeasurementLevel level);
/// @param calibration A calibration state.
/// @return The calibration word: not_needed, needed or not_possible.
const char* CalibrationWord(Calibration calibration);
/// @param type A device type.
/// @return The device type word: laptop or desktop.
const char* DeviceTypeWord(DeviceType type);
/// @param state An NVML state.
/// @return The NVML word: ok, unavailable or error.
const char* NvmlWord(NvmlState state);

/// Waits 3 seconds with a 1 second processor load in between (SPEC 3.2, step 3). Tests use a fake that does not wait.
class IScanWaiter {
public:
    virtual ~IScanWaiter() = default;
    /// Waits for the 3 seconds. It always returns after the wait; it cannot fail.
    virtual void WaitWithLoad() = 0;
};

/// The real waiter: 1 second of sleep, 1 second of a spinning loop on the calling thread, 1 second of sleep.
class SystemScanWaiter final : public IScanWaiter {
public:
    void WaitWithLoad() override;
};

/// The sources a scan reads. The caller owns them; they must outlive the call.
struct ScanSources {
    IMachineInfoSource& machine;
    IEmiSource& emi;
    IBatterySource& battery;
    IPowerMeterSource& powerMeter;
    IDisplaySource& display;
    IDisplayLayoutSource& displayLayout;
    IGpuSource& gpu;
    IGpuUseSource& gpuUse;
    IIntelDttSource& intelDtt;
    IPawnIoSource& pawnIo;
    IScanWaiter& waiter;
    Reading<bool> (*onBattery)();  ///< The on-battery reading (ReadOnBattery in the real program).
};

/// Reads the machine facts. A failed read gives empty facts: every field unknown.
/// @param source The machine source.
/// @return The facts.
MachineInfo ReadMachineFacts(IMachineInfoSource& source);

/// Builds the battery result from one read of all batteries (SPEC 3.3, 3.5).
/// @param reading The battery read.
/// @return The result; a failed read gives unknown values, never "does not exist".
BatteryResult BuildBatteryResult(const Reading<std::vector<BatteryState>>& reading);

/// Builds the display result from the brightness, the power source and the layout.
/// @param brightness The brightness read.
/// @param onBattery The on-battery read.
/// @param layout The display layout read. A failed layout leaves displays, external_displays and refresh_hz out.
/// @return The result.
DisplayResult BuildDisplayResult(const Reading<DisplayBrightness>& brightness, const Reading<bool>& onBattery,
                                 const Reading<DisplayLayout>& layout);

/// Runs the whole scan: tests every source with the four steps of SPEC 3.2 and decides level, calibration and device type.
/// One failed source never stops the others. A failed read gives unknown, never "does not exist".
/// @param sources The sources and the waiter.
/// @param context The context to record.
/// @param scannedUtc The time text to record, "YYYY-MM-DDTHH:MM:SSZ".
/// @return The result. It always holds every source, in the order of SPEC 3.5.
ScanResult RunScan(const ScanSources& sources, ScanContext context, const std::string& scannedUtc);

}  // namespace ews
