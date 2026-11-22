// scan_run.cpp: RunScan, the test of every source with the four steps of SPEC 3.2 (see ews/scan.h).
// Used by ews-scan. Every source has one small Build function that turns its readings into the result.
// A failed reading gives unknown values, never "does not exist" and never 0. No device path or serial number is kept.
#include <algorithm>
#include <cmath>
#include <numeric>
#include <optional>
#include <vector>

#include "ews/gpu_link.h"
#include "ews/scan.h"

namespace ews {

namespace {

constexpr std::uint32_t kMaxUint32 = 0xFFFFFFFFu;

/// Maps a failed reading to the common steps (SPEC 3.2).
/// Unavailable: the source is not there. Denied: it is there, but not readable. Any other failure: unknown if it is there.
/// @param status The status of the failed reading.
/// @param error The Windows error code of the failure.
/// @return Common steps with present, readable and the error code set; live and sample stay unknown.
SourceCommon FailureCommon(Status status, unsigned long error) {
    SourceCommon common;
    common.error = error;
    common.readable = Tri::False;
    switch (status) {
        case Status::Unavailable: common.present = Tri::False; break;
        case Status::Denied: common.present = Tri::True; break;
        default: common.present = Tri::Unknown; break;
    }
    return common;
}

/// Common steps of a source that is not there although nothing failed (an empty list).
SourceCommon AbsentCommon() {
    SourceCommon common;
    common.present = Tri::False;
    common.readable = Tri::False;
    return common;
}

/// @return A finite number as an optional, or nothing for NaN and infinity (untrusted values never reach the output).
std::optional<double> FiniteOrNothing(double value) {
    return std::isfinite(value) ? std::optional<double>(value) : std::nullopt;
}

// ---- machine ----

/// Reads the machine facts. A failed read gives empty facts: every field unknown.
MachineInfo ReadMachine(IMachineInfoSource& source) {
    auto reading = source.Read();
    return reading.ok() ? reading.value : MachineInfo{};
}

// ---- EMI ----

/// @return The sample at `position`, or an unknown sample when the read failed or the list is shorter.
EmiSample SampleAt(const Reading<std::vector<EmiSample>>& reading, std::size_t position) {
    bool usable = reading.ok() && position < reading.value.size();
    return usable ? reading.value[position] : EmiSample{};
}

/// Decides if one channel is live: its energy counter grew between the two reads.
/// A missing sample or a counter that went back is unknown.
Tri ChannelLive(const EmiSample& before, const EmiSample& after) {
    if (!before.known || !after.known) {
        return Tri::Unknown;
    }
    if (after.energyPicowattHours < before.energyPicowattHours) {
        return Tri::Unknown;
    }
    return after.energyPicowattHours > before.energyPicowattHours ? Tri::True : Tri::False;
}

/// Builds the result of one channel from its two samples.
EmiChannelResult BuildChannel(const EmiChannel& channel, const EmiSample& before, const EmiSample& after) {
    EmiChannelResult result;
    result.device = channel.deviceIndex;
    result.index = channel.channelIndex;
    result.name = channel.name;
    result.kind = channel.kind;
    result.live = ChannelLive(before, after);
    auto power = EmiPowerWatts(before, after);
    result.sampleWatts = power.ok() ? FiniteOrNothing(power.value) : std::nullopt;
    return result;
}

/// Builds every channel result, in the order of the enumeration.
std::vector<EmiChannelResult> BuildChannels(const std::vector<EmiChannel>& channels, const Reading<std::vector<EmiSample>>& first,
                                            const Reading<std::vector<EmiSample>>& second) {
    std::vector<EmiChannelResult> results;
    for (std::size_t i = 0; i < channels.size(); ++i) {
        results.push_back(BuildChannel(channels[i], SampleAt(first, i), SampleAt(second, i)));
    }
    return results;
}

/// @return True when the sample is known.
bool SampleIsKnown(const EmiSample& sample) { return sample.known; }

/// @return True when at least one sample of the reading is known.
bool AnySampleKnown(const Reading<std::vector<EmiSample>>& reading) {
    return reading.ok() && std::any_of(reading.value.begin(), reading.value.end(), SampleIsKnown);
}

/// A predicate: the channel has this live value.
struct HasLive {
    Tri wanted;
    bool operator()(const EmiChannelResult& channel) const { return channel.live == wanted; }
};

/// The live word of the whole source: true when one channel is live, unknown when one is unknown, else false.
Tri EmiLive(const std::vector<EmiChannelResult>& channels) {
    if (std::any_of(channels.begin(), channels.end(), HasLive{Tri::True})) {
        return Tri::True;
    }
    return std::any_of(channels.begin(), channels.end(), HasLive{Tri::Unknown}) ? Tri::Unknown : Tri::False;
}

/// @return The larger of the accumulated power and the power of the channel (unknown power counts as nothing).
double LargerPower(double largest, const EmiChannelResult& channel) { return (std::max)(largest, channel.sampleWatts.value_or(0.0)); }

/// Builds the EMI result from Open and the two reads.
EmiResult BuildEmi(const Reading<std::vector<EmiChannel>>& open, const Reading<std::vector<EmiSample>>& first,
                   const Reading<std::vector<EmiSample>>& second) {
    EmiResult result;
    if (!open.ok()) {
        result.common = FailureCommon(open.status, open.error);
        return result;
    }
    if (open.value.empty()) {
        result.common = AbsentCommon();
        return result;
    }
    result.common.present = Tri::True;
    result.channels = BuildChannels(open.value, first, second);
    if (!AnySampleKnown(first)) {
        result.common.readable = Tri::False;
        result.common.error = first.error;
        return result;
    }
    result.common.readable = Tri::True;
    result.common.live = EmiLive(result.channels);
    result.common.sample = std::accumulate(result.channels.begin(), result.channels.end(), 0.0, LargerPower);
    return result;
}

// ---- battery ----

/// @return True when the battery has unit mWh.
bool IsMwh(const BatteryState& battery) { return battery.unit == BatteryUnit::MilliwattHours; }

/// @return True when the battery has unit mWh and a known full charged capacity.
bool IsMwhWithFullCharge(const BatteryState& battery) { return IsMwh(battery) && battery.fullChargedKnown; }

/// @return True when the battery reports its rate.
bool HasRate(const BatteryState& battery) { return battery.rateKnown; }

/// @return The full charged capacity in mWh when it counts (mWh unit, known), else 0.
std::uint64_t CountedFullMwh(std::uint64_t total, const BatteryState& battery) {
    return IsMwhWithFullCharge(battery) ? total + battery.fullCharged : total;
}

/// @return True when the battery has unit mWh and a known designed capacity.
bool IsMwhWithDesign(const BatteryState& battery) { return IsMwh(battery) && battery.designedKnown; }

/// @return The designed capacity in mWh when it counts (mWh unit, known), else the total unchanged.
std::uint64_t CountedDesignMwh(std::uint64_t total, const BatteryState& battery) {
    return IsMwhWithDesign(battery) ? total + battery.designed : total;
}

/// @return The larger of the accumulated cycle count and the cycle count of the battery.
std::uint32_t LargerCycleCount(std::uint32_t largest, const BatteryState& battery) { return (std::max)(largest, battery.cycleCount); }

/// Fills the designed capacity and the cycle count (SPEC 1.3). Unknown values are left out.
void SummariseDesignAndCycles(const std::vector<BatteryState>& batteries, BatteryResult& result) {
    if (std::any_of(batteries.begin(), batteries.end(), IsMwhWithDesign)) {
        std::uint64_t design = std::accumulate(batteries.begin(), batteries.end(), std::uint64_t{0}, CountedDesignMwh);
        result.designMwh = static_cast<std::uint32_t>((std::min<std::uint64_t>)(design, kMaxUint32));
    }
    std::uint32_t cycles = std::accumulate(batteries.begin(), batteries.end(), std::uint32_t{0}, LargerCycleCount);
    if (cycles != 0) {
        result.cycleCount = cycles;
    }
}

/// Fills the summary of all batteries into the result.
void SummariseBatteries(const std::vector<BatteryState>& batteries, BatteryResult& result) {
    SummariseDesignAndCycles(batteries, result);
    result.count = static_cast<std::uint32_t>((std::min<std::size_t>)(batteries.size(), kMaxUint32));
    bool anyMwh = std::any_of(batteries.begin(), batteries.end(), IsMwh);
    if (anyMwh) {
        result.unit = BatteryUnit::MilliwattHours;
    }
    if (!anyMwh) {
        result.unit = BatteryUnit::Relative;
    }
    result.rateKnown = std::any_of(batteries.begin(), batteries.end(), HasRate);
    result.reportsMwh = std::any_of(batteries.begin(), batteries.end(), IsMwhWithFullCharge);
    if (!result.reportsMwh) {
        return;
    }
    std::uint64_t total = std::accumulate(batteries.begin(), batteries.end(), std::uint64_t{0}, CountedFullMwh);
    result.fullMwh = static_cast<std::uint32_t>((std::min<std::uint64_t>)(total, kMaxUint32));
    result.common.sample = static_cast<double>(total);
}

/// Builds the battery result. The rate and the charge are read once; battery has no live step (live stays unknown).
BatteryResult BuildBattery(const Reading<std::vector<BatteryState>>& reading) {
    BatteryResult result;
    if (!reading.ok()) {
        result.common = FailureCommon(reading.status, reading.error);
        return result;
    }
    if (reading.value.empty()) {
        result.common = AbsentCommon();
        return result;
    }
    result.common.present = Tri::True;
    result.common.readable = Tri::True;
    SummariseBatteries(reading.value, result);
    return result;
}

// ---- ACPI power meter ----

using MeterReading = Reading<std::vector<PowerMeterInstance>>;

/// @return The reading, but an empty list counts as Unavailable (no instance means no meter).
MeterReading Normalised(const MeterReading& reading) {
    bool empty = reading.ok() && reading.value.empty();
    return empty ? MeterReading::Failure(Status::Unavailable, 0) : reading;
}

/// @return The larger of the accumulated value and this instance (NaN and infinity count as nothing).
double LargerValue(double largest, const PowerMeterInstance& instance) {
    return std::isfinite(instance.value) ? (std::max)(largest, instance.value) : largest;
}

/// @return The largest value of the reading, 0 when there is none.
double LargestValue(const MeterReading& reading) {
    return std::accumulate(reading.value.begin(), reading.value.end(), 0.0, LargerValue);
}

/// @return The reading to report a failure from: the first one, unless it only says "not there" and the second says more.
const MeterReading& WorstOf(const MeterReading& first, const MeterReading& second) {
    return first.status == Status::Unavailable ? second : first;
}

/// Fills the result when at least one of the two reads gave values.
void FillMeterValues(const MeterReading& first, const MeterReading& second, PowerMeterResult& result) {
    double largest = (std::max)(first.ok() ? LargestValue(first) : 0.0, second.ok() ? LargestValue(second) : 0.0);
    const MeterReading& shown = first.ok() ? first : second;
    result.common.present = Tri::True;
    result.common.readable = Tri::True;
    result.instances = static_cast<std::uint32_t>((std::min<std::size_t>)(shown.value.size(), kMaxUint32));
    result.common.sample = largest;
    bool bothRead = first.ok() && second.ok();
    result.common.live = largest > 0.0 ? Tri::True : (bothRead ? Tri::False : Tri::Unknown);
}

/// Builds the power meter result from the two reads.
PowerMeterResult BuildPowerMeter(const MeterReading& rawFirst, const MeterReading& rawSecond) {
    PowerMeterResult result;
    MeterReading first = Normalised(rawFirst);
    MeterReading second = Normalised(rawSecond);
    if (!first.ok() && !second.ok()) {
        const MeterReading& worst = WorstOf(first, second);
        result.common = FailureCommon(worst.status, worst.error);
        return result;
    }
    FillMeterValues(first, second, result);
    return result;
}

// ---- display ----

/// Builds the display result. The brightness is taken for the power source that is active now;
/// when that is unknown or the value is out of range, the percentage is left out.
DisplayResult BuildDisplay(const Reading<DisplayBrightness>& reading, const Reading<bool>& onBattery) {
    DisplayResult result;
    if (!reading.ok()) {
        result.common = FailureCommon(reading.status, reading.error);
        return result;
    }
    result.common.present = Tri::True;
    result.common.readable = Tri::True;
    if (!onBattery.ok()) {
        return result;
    }
    auto percent = BrightnessPercent(reading.value, onBattery.value);
    if (!percent.ok()) {
        return result;
    }
    result.brightnessPercent = percent.value;
    result.common.sample = static_cast<double>(percent.value);
    return result;
}

/// Copies the display layout into the result; a failed layout read leaves the three fields out.
void ApplyLayout(const Reading<DisplayLayout>& layout, DisplayResult& result) {
    if (!layout.ok()) {
        return;
    }
    result.displays = layout.value.displays;
    result.externalDisplays = layout.value.externalDisplays;
    result.refreshHz = layout.value.refreshHz;
}

// ---- graphics ----

/// @return The larger of the accumulated power and this device, in watts; an unknown device counts as nothing.
double LargerDevicePower(double largest, const NvmlPower& device) {
    return device.known ? (std::max)(largest, device.milliwatts / 1000.0) : largest;
}

/// @return True when NVML knows the power of this device.
bool PowerKnown(const NvmlPower& device) { return device.known; }

/// Fills the NVML fields of the graphics result.
void ApplyNvml(const Reading<std::vector<NvmlPower>>& reading, GpuResult& result) {
    if (reading.status == Status::Unavailable) {
        result.nvml = NvmlState::Unavailable;
        return;
    }
    if (!reading.ok()) {
        result.nvml = NvmlState::Error;
        return;
    }
    result.nvml = NvmlState::Ok;
    result.nvmlDevices = static_cast<std::uint32_t>((std::min<std::size_t>)(reading.value.size(), kMaxUint32));
    bool anyKnown = std::any_of(reading.value.begin(), reading.value.end(), PowerKnown);
    if (anyKnown) {
        result.common.sample = std::accumulate(reading.value.begin(), reading.value.end(), 0.0, LargerDevicePower);
    }
}

/// Builds the list entry of one NVML device.
NvmlEntry BuildNvmlEntry(const NvmlDeviceFacts& device, const std::optional<std::size_t>& adapter) {
    NvmlEntry entry;
    entry.name = device.name;
    entry.adapter = adapter;
    entry.power = device.powerAnswered;
    entry.energy = device.energyAnswered;
    if (device.powerAnswered && device.milliwatts.has_value()) {
        entry.sampleWatts = FiniteOrNothing(*device.milliwatts / 1000.0);
    }
    return entry;
}

/// Fills the list of NVML devices (SPEC 1.5). A failed read leaves the list empty.
void ApplyNvmlEntries(const Reading<std::vector<NvmlDeviceFacts>>& devices, GpuResult& result) {
    if (!devices.ok()) {
        return;
    }
    auto links = LinkNvmlToAdapters(devices.value, result.adapters);
    for (std::size_t i = 0; i < devices.value.size(); ++i) {
        result.nvmlEntries.push_back(BuildNvmlEntry(devices.value[i], links[i]));
    }
}

/// Builds the graphics result from the adapter list, the NVML power and the NVML device facts.
GpuResult BuildGpu(const Reading<std::vector<GpuAdapter>>& adapters, const Reading<std::vector<NvmlPower>>& nvml,
                   const Reading<std::vector<NvmlDeviceFacts>>& devices) {
    GpuResult result;
    if (!adapters.ok()) {
        result.common = FailureCommon(adapters.status, adapters.error);
    }
    if (adapters.ok()) {
        result.common.present = adapters.value.empty() ? Tri::False : Tri::True;
        result.common.readable = Tri::True;
        result.adapters = adapters.value;
    }
    ApplyNvml(nvml, result);
    ApplyNvmlEntries(devices, result);
    return result;
}

// ---- GPU use ----

/// @return The larger of the accumulated use and the use of the adapter (an unknown use counts as nothing).
double LargerUse(double largest, const AdapterUse& use) { return (std::max)(largest, use.percent.value_or(0.0)); }

/// @return True when the adapter has a known use.
bool UseKnown(const AdapterUse& use) { return use.percent.has_value(); }

/// Fills the fields of the GPU use result from the instances of one read.
void FillGpuUse(const MeterReading& reading, GpuUseResult& result) {
    GpuUseSummary summary = SummariseGpuUse(reading.value);
    result.common.present = Tri::True;
    result.common.readable = Tri::True;
    result.adapters = static_cast<std::uint32_t>((std::min<std::size_t>)(summary.adapters.size(), kMaxUint32));
    if (std::any_of(summary.adapters.begin(), summary.adapters.end(), UseKnown)) {
        result.common.sample = std::accumulate(summary.adapters.begin(), summary.adapters.end(), 0.0, LargerUse);
    }
}

/// Builds the GPU use result (SPEC 1.5). The first read only primes a rate counter, so the second read gives the value;
/// the first is used only when the second failed. live stays unknown (SPEC 3.2).
GpuUseResult BuildGpuUse(const MeterReading& rawFirst, const MeterReading& rawSecond) {
    GpuUseResult result;
    MeterReading first = Normalised(rawFirst);
    MeterReading second = Normalised(rawSecond);
    if (!first.ok() && !second.ok()) {
        result.common = FailureCommon(second.status, second.error);
        return result;
    }
    FillGpuUse(second.ok() ? second : first, result);
    return result;
}

// ---- Intel DTT ----

/// Builds the Intel DTT result: present when the service exists.
IntelDttResult BuildIntelDtt(const Reading<bool>& reading) {
    IntelDttResult result;
    if (!reading.ok()) {
        result.common = FailureCommon(reading.status, reading.error);
        return result;
    }
    result.common.present = reading.value ? Tri::True : Tri::False;
    result.common.readable = Tri::True;
    return result;
}

// ---- PawnIO ----

/// @return True when the device open proves that PawnIO exists (it opened, or Windows refused access).
bool DeviceProvesPresence(PawnIoDevice device) { return device == PawnIoDevice::Opened || device == PawnIoDevice::Denied; }

/// Decides "present" (SPEC 3.5): true when the registry value exists or the device opened or was denied;
/// false when the registry says missing and the device was not found; otherwise unknown.
Tri PawnIoPresent(const PawnIoState& state) {
    if (state.registry == PawnIoRegistry::Found || DeviceProvesPresence(state.device)) {
        return Tri::True;
    }
    bool bothMissing = state.registry == PawnIoRegistry::Missing && state.device == PawnIoDevice::NotFound;
    return bothMissing ? Tri::False : Tri::Unknown;
}

/// Decides "readable": only when PawnIO is present. True when the device opened; false when it was denied or not found;
/// unknown after any other error.
Tri PawnIoReadable(Tri present, PawnIoDevice device) {
    if (present != Tri::True) {
        return Tri::Unknown;
    }
    if (device == PawnIoDevice::Opened) {
        return Tri::True;
    }
    bool refused = device == PawnIoDevice::Denied || device == PawnIoDevice::NotFound;
    return refused ? Tri::False : Tri::Unknown;
}

/// Builds the PawnIO result from one read (SPEC 3.3, 3.5). The error code is the one of the device open.
/// The version text comes from the registry, so it is used only when IsValidPawnIoVersion says yes.
PawnIoResult BuildPawnIo(const Reading<PawnIoState>& reading) {
    PawnIoResult result;
    if (!reading.ok()) {
        result.common.error = reading.error;
        return result;
    }
    const PawnIoState& state = reading.value;
    result.common.present = PawnIoPresent(state);
    result.common.readable = PawnIoReadable(result.common.present, state.device);
    result.common.error = state.deviceError;
    if (state.versionText.has_value() && IsValidPawnIoVersion(*state.versionText)) {
        result.version = state.versionText;
    }
    return result;
}
// ---- decisions ----

/// @return True when the channel is live and has this kind.
struct LiveOfKind {
    EmiKind kind;
    bool operator()(const EmiChannelResult& channel) const { return channel.kind == kind && channel.live == Tri::True; }
};

/// Collects the facts of the level rule from the finished source results.
LevelFacts FactsOf(const ScanResult& result) {
    const auto& channels = result.emi.channels;
    LevelFacts facts;
    facts.liveSystemChannel = std::any_of(channels.begin(), channels.end(), LiveOfKind{EmiKind::System});
    facts.liveCpuPackage = std::any_of(channels.begin(), channels.end(), LiveOfKind{EmiKind::CpuPackage});
    facts.mwhBattery = result.battery.reportsMwh;
    return facts;
}

/// Sets level, calibration and device type of the result.
void Decide(ScanResult& result) {
    result.level = DecideLevel(FactsOf(result));
    result.calibration = DecideCalibration(result.level, result.battery.reportsMwh);
    result.deviceType = DecideDeviceType(result.machine.smbios.chassisType, result.battery.common.present == Tri::True);
}

}  // namespace

MachineInfo ReadMachineFacts(IMachineInfoSource& source) { return ReadMachine(source); }

BatteryResult BuildBatteryResult(const Reading<std::vector<BatteryState>>& reading) { return BuildBattery(reading); }

DisplayResult BuildDisplayResult(const Reading<DisplayBrightness>& brightness, const Reading<bool>& onBattery,
                                 const Reading<DisplayLayout>& layout) {
    DisplayResult result = BuildDisplay(brightness, onBattery);
    ApplyLayout(layout, result);
    return result;
}

ScanResult RunScan(const ScanSources& sources, ScanContext context, const std::string& scannedUtc) {
    ScanResult result;
    result.scannedUtc = scannedUtc;
    result.context = context;
    result.machine = ReadMachine(sources.machine);

    // Two reads of the counters, with the 3 second wait and the processor load between them.
    auto emiOpen = sources.emi.Open();
    auto emiFirst = sources.emi.Read();
    auto meterFirst = sources.powerMeter.Read();
    auto gpuUseFirst = sources.gpuUse.Read();
    sources.waiter.WaitWithLoad();
    auto emiSecond = sources.emi.Read();
    auto meterSecond = sources.powerMeter.Read();
    auto gpuUseSecond = sources.gpuUse.Read();

    result.emi = BuildEmi(emiOpen, emiFirst, emiSecond);
    result.battery = BuildBattery(sources.battery.Read());
    result.powerMeter = BuildPowerMeter(meterFirst, meterSecond);
    result.display = BuildDisplayResult(sources.display.Read(), sources.onBattery(), sources.displayLayout.Read());
    result.gpu = BuildGpu(sources.gpu.ListAdapters(), sources.gpu.ReadNvmlPower(), sources.gpu.ReadNvmlDevices());
    result.gpuUse = BuildGpuUse(gpuUseFirst, gpuUseSecond);
    result.intelDtt = BuildIntelDtt(sources.intelDtt.IsPresent());
    result.pawnIo = BuildPawnIo(sources.pawnIo.Read());
    Decide(result);
    return result;
}

}  // namespace ews
