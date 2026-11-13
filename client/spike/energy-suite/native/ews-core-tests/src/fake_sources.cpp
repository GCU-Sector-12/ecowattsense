// fake_sources.cpp: the Scenario constructor and the small builders declared in fake_sources.h.
// Used by the scan tests only.
#include "fake_sources.h"

namespace ews_test {

namespace {

constexpr std::uint64_t kBaseEnergy = 1'000'000'000'000ULL;  // pWh, an arbitrary start of the counter
constexpr std::uint64_t kGrowth = 3'416'667'000ULL;          // pWh, about 4.1 W over 3 s
constexpr std::uint64_t kStart = 1'000'000'000ULL;           // 100 ns units
constexpr std::uint64_t kThreeSeconds = 30'000'000ULL;       // 100 ns units

/// Adds the channel, and both of its samples, to a scenario's EMI fake.
void AddChannel(Scenario& scenario, std::size_t position, const std::string& name, bool live) {
    auto device = static_cast<std::uint32_t>(0);
    auto index = static_cast<std::uint32_t>(position);
    scenario.emi.open.value.push_back(MakeChannel(device, index, name));
    scenario.emi.first.value.push_back(MakeSample(kBaseEnergy, kStart));
    std::uint64_t growth = live ? kGrowth : 0;
    scenario.emi.second.value.push_back(MakeSample(kBaseEnergy + growth, kStart + kThreeSeconds));
}

}  // namespace

ews::EmiChannel MakeChannel(std::uint32_t device, std::uint32_t index, const std::string& name) {
    ews::EmiChannel channel;
    channel.deviceIndex = device;
    channel.channelIndex = index;
    channel.name = name;
    channel.kind = ews::ClassifyEmiChannel(name);
    return channel;
}

ews::EmiSample MakeSample(std::uint64_t picowattHours, std::uint64_t time100ns) {
    ews::EmiSample sample;
    sample.known = true;
    sample.energyPicowattHours = picowattHours;
    sample.time100ns = time100ns;
    return sample;
}

ews::BatteryState MakeMwhBattery(bool fullKnown) {
    ews::BatteryState battery;
    battery.unit = ews::BatteryUnit::MilliwattHours;
    battery.fullChargedKnown = fullKnown;
    battery.fullCharged = fullKnown ? 36092 : 0;
    return battery;
}

void SetEmiChannels(Scenario& scenario, const std::vector<std::string>& names, const std::vector<bool>& live) {
    scenario.emi.open = ews::Reading<std::vector<ews::EmiChannel>>::Success({});
    scenario.emi.first = ews::Reading<std::vector<ews::EmiSample>>::Success({});
    scenario.emi.second = ews::Reading<std::vector<ews::EmiSample>>::Success({});
    for (std::size_t i = 0; i < names.size(); ++i) {
        AddChannel(scenario, i, names[i], live[i]);
    }
}

Scenario::Scenario() {
    ews::MachineInfo info;
    info.smbios.maker = "HP";
    info.smbios.model = "HP 250 G7 Notebook PC";
    info.smbios.chassisType = 10;
    info.processorVendor = "GenuineIntel";
    info.processorBrand = "Test Processor";
    info.logicalProcessors = 8;
    info.windowsBuild = 26300;
    machine.result = ews::Reading<ews::MachineInfo>::Success(info);

    SetEmiChannels(*this, {"RAPL_Package0_PKG", "RAPL_Package0_DRAM", "RAPL_Package0_PP0", "RAPL_Package0_PP1"},
                   {true, true, true, true});

    battery.result = ews::Reading<std::vector<ews::BatteryState>>::Success({MakeMwhBattery(true)});

    std::vector<ews::PowerMeterInstance> idle{{"System", 0.0}};
    powerMeter.first = ews::Reading<std::vector<ews::PowerMeterInstance>>::Success(idle);
    powerMeter.second = ews::Reading<std::vector<ews::PowerMeterInstance>>::Success(idle);

    display.result = ews::Reading<ews::DisplayBrightness>::Failure(ews::Status::Denied, 5);

    ews::GpuAdapter adapter;
    adapter.description = "Test Graphics";
    adapter.vendorId = 0x8086;
    adapter.dedicatedMemoryMegabytes = 128;
    gpu.adapters = ews::Reading<std::vector<ews::GpuAdapter>>::Success({adapter});
    gpu.nvml = ews::Reading<std::vector<ews::NvmlPower>>::Failure(ews::Status::Unavailable, 0);

    dtt.result = ews::Reading<bool>::Success(false);
}

}  // namespace ews_test
