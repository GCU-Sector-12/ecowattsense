// watch_session.cpp: the watch session of ews-scan (see ews/watch_session.h).
// Used by ews-scan and the tests. A failed read is "unknown": an empty list in the header or a null in a line.
// Nothing here waits or touches a clock: the caller passes the time of each period.

#include "ews/watch_session.h"

#include "ews/scan.h"
#include "ews/scan_output.h"
#include "ews/watch_line.h"

namespace ews {

namespace {

/// @return The EMI channels of a reading, or none when the open failed.
std::vector<EmiChannel> ChannelsOf(const Reading<std::vector<EmiChannel>>& opened) {
    return opened.ok() ? opened.value : std::vector<EmiChannel>();
}

/// @return The adapters of a reading, or none when the list failed.
std::vector<GpuAdapter> AdaptersOf(const Reading<std::vector<GpuAdapter>>& listed) {
    return listed.ok() ? listed.value : std::vector<GpuAdapter>();
}

/// @return The header entries of the EMI channels.
std::vector<WatchEmiEntry> EmiEntries(const std::vector<EmiChannel>& channels) {
    std::vector<WatchEmiEntry> entries;
    for (const EmiChannel& channel : channels) {
        entries.push_back(WatchEmiEntry{channel.name, EmiKindWord(channel.kind), channel.deviceIndex, channel.channelIndex});
    }
    return entries;
}

/// @return The header entries of the NVML devices; `links` holds the adapter of each one.
std::vector<WatchNvmlEntry> NvmlEntries(const std::vector<NvmlDeviceFacts>& devices,
                                        const std::vector<std::optional<std::size_t>>& links) {
    std::vector<WatchNvmlEntry> entries;
    for (std::size_t i = 0; i < devices.size(); ++i) {
        entries.push_back(WatchNvmlEntry{devices[i].name, links[i], devices[i].powerAnswered, devices[i].energyAnswered});
    }
    return entries;
}

/// @return The role word of a target, or nothing.
std::optional<std::string> RoleWordOf(const GpuUseTarget& target) {
    return target.role.has_value() ? std::optional<std::string>(GpuRoleWord(*target.role)) : std::nullopt;
}

/// @return The header entries of the adapters whose use is read.
std::vector<WatchGpuUseEntry> GpuUseEntries(const std::vector<GpuUseTarget>& targets) {
    std::vector<WatchGpuUseEntry> entries;
    for (const GpuUseTarget& target : targets) {
        entries.push_back(WatchGpuUseEntry{target.adapter, RoleWordOf(target), target.vendorId});
    }
    return entries;
}

/// @return One flag per NVML device: its energy counter answers.
std::vector<bool> EnergyFlags(const std::vector<NvmlDeviceFacts>& devices) {
    std::vector<bool> flags;
    for (const NvmlDeviceFacts& device : devices) {
        flags.push_back(device.energyAnswered);
    }
    return flags;
}

/// Fills the three objects of capabilities.json that the header repeats.
void FillStaticObjects(const WatchSources& sources, WatchHeader& header) {
    header.machineJson = MachineObjectJson(ReadMachineFacts(sources.machine));
    header.batteryJson = BatteryObjectJson(BuildBatteryResult(sources.battery.Read()));
    header.displayJson = DisplayObjectJson(BuildDisplayResult(sources.display.Read(), sources.onBattery(), sources.displayLayout.Read()));
}

/// @return The energy of one channel in microjoules, or nothing when the sample is unknown.
std::optional<std::uint64_t> EnergyOf(const EmiSample& sample) {
    if (!sample.known) {
        return std::nullopt;
    }
    return PicowattHoursToMicrojoules(sample.energyPicowattHours).value;
}

/// @return The energy of each of `count` channels; a failed read, or a short list, gives nothing for the rest.
std::vector<std::optional<std::uint64_t>> EmiMicrojoules(const Reading<std::vector<EmiSample>>& reading, std::size_t count) {
    std::vector<std::optional<std::uint64_t>> values(count);
    for (std::size_t i = 0; i < count && reading.ok() && i < reading.value.size(); ++i) {
        values[i] = EnergyOf(reading.value[i]);
    }
    return values;
}

/// Copies the NVML samples into the input, one entry per device.
void ApplyNvmlSamples(const Reading<std::vector<NvmlSample>>& reading, std::size_t count, WatchInput& input) {
    input.nvmlMw.assign(count, std::nullopt);
    input.nvmlMj.assign(count, std::nullopt);
    for (std::size_t i = 0; i < count && reading.ok() && i < reading.value.size(); ++i) {
        input.nvmlMw[i] = reading.value[i].milliwatts;
        input.nvmlMj[i] = reading.value[i].millijoules;
    }
}

/// @return The use of each target adapter; a failed read gives nothing for all of them.
std::vector<std::optional<double>> GpuPercents(const Reading<std::vector<PdhInstance>>& reading,
                                               const std::vector<GpuUseTarget>& targets) {
    std::vector<std::optional<double>> values(targets.size());
    GpuUseSummary summary = SummariseGpuUse(reading.ok() ? reading.value : std::vector<PdhInstance>());
    for (std::size_t i = 0; i < targets.size(); ++i) {
        values[i] = GpuUseOfAdapter(summary, targets[i].luid);
    }
    return values;
}

}  // namespace

WatchSession::WatchSession(const WatchSources& sources, std::uint32_t periodMs)
    : sources_(sources), periodMs_(periodMs), engine_(0, {}, 0) {}

std::string WatchSession::Start() {
    WatchHeader header;
    header.periodMs = periodMs_;
    FillStaticObjects(sources_, header);
    channels_ = ChannelsOf(sources_.emi.Open());
    header.emi = EmiEntries(channels_);
    std::vector<GpuAdapter> adapters = AdaptersOf(sources_.gpu.ListAdapters());
    targets_ = HardwareAdapterTargets(adapters);
    header.gpuUse = GpuUseEntries(targets_);
    auto devices = sources_.nvml.Open();
    nvmlOpen_ = devices.ok();
    nvmlDevices_ = nvmlOpen_ ? devices.value : std::vector<NvmlDeviceFacts>();
    header.nvml = NvmlEntries(nvmlDevices_, LinkNvmlToAdapters(nvmlDevices_, adapters));
    sources_.gpuUse.Read();  // a rate counter needs two collections: this one only primes the query
    engine_ = WatchEngine(channels_.size(), EnergyFlags(nvmlDevices_), targets_.size());
    return BuildWatchHeaderLine(header);
}

std::string WatchSession::Sample(std::int64_t tMs) {
    WatchInput input;
    input.tMs = tMs;
    if (!channels_.empty()) {
        input.emiUj = EmiMicrojoules(sources_.emi.Read(), channels_.size());
    }
    if (nvmlOpen_) {
        ApplyNvmlSamples(sources_.nvml.Read(), nvmlDevices_.size(), input);
    }
    input.gpuPct = GpuPercents(sources_.gpuUse.Read(), targets_);
    return BuildWatchSampleLine(engine_.Step(input));
}

}  // namespace ews
