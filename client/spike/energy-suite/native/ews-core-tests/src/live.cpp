// live.cpp: the --live mode of ews-core-tests. It reads this computer with the real readers and prints the values.
#include "live.h"

#include <windows.h>

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "ews/battery.h"
#include "ews/cpu_times.h"
#include "ews/display.h"
#include "ews/emi.h"
#include "ews/gpu.h"
#include "ews/intel_dtt.h"
#include "ews/machine_info.h"
#include "ews/power_meter.h"

namespace ews_test {

namespace {

/// @return A fixed word for a status.
const char* StatusWord(ews::Status status) {
    switch (status) {
        case ews::Status::Ok: return "ok";
        case ews::Status::Unavailable: return "unavailable";
        case ews::Status::Denied: return "denied";
        default: return "error";
    }
}

/// Prints the processor load measured over one second.
void PrintCpu() {
    ews::SystemCpuTimesSource source;
    auto first = source.Read();
    Sleep(1000);  // one second between the two samples
    auto second = source.Read();
    if (!first.ok() || !second.ok()) {
        std::printf("cpu: read failed (%s, error %lu)\n", StatusWord(second.status), second.error);
        return;
    }
    auto percent = ews::CpuPercent(first.value, second.value);
    std::printf("cpu: %s %.1f %%\n", StatusWord(percent.status), percent.value);
}

/// @return The word "known" or "unknown" for a flag, so an unknown value is never shown as a plain 0.
const char* KnownWord(bool known) { return known ? "known" : "unknown"; }

/// Prints one battery on one line.
void PrintBattery(std::size_t index, const ews::BatteryState& b) {
    const char* unit = b.unit == ews::BatteryUnit::MilliwattHours ? "mWh" : "relative";
    std::printf("battery %zu: unit=%s remaining=%u(%s) full=%u(%s) designed=%u(%s) rate=%d(%s) voltage=%u(%s)\n", index, unit,
                b.remaining, KnownWord(b.remainingKnown), b.fullCharged, KnownWord(b.fullChargedKnown), b.designed,
                KnownWord(b.designedKnown), b.rate, KnownWord(b.rateKnown), b.voltage, KnownWord(b.voltageKnown));
    std::printf("battery %zu: onLine=%d charging=%d discharging=%d\n", index, b.onLine, b.charging, b.discharging);
}

/// Prints every battery, or the status when the read failed.
void PrintBatteries() {
    ews::SystemBatterySource source;
    auto batteries = source.Read();
    if (!batteries.ok()) {
        std::printf("battery: %s (error %lu)\n", StatusWord(batteries.status), batteries.error);
        return;
    }
    std::printf("battery count: %zu\n", batteries.value.size());
    for (std::size_t i = 0; i < batteries.value.size(); ++i) {
        PrintBattery(i, batteries.value[i]);
    }
}

/// Prints the on-battery flag.
void PrintOnBattery() {
    auto on = ews::ReadOnBattery();
    std::printf("on battery: %s %d\n", StatusWord(on.status), on.value ? 1 : 0);
}

/// @return The text of an optional string, or "unknown".
std::string TextOr(const std::optional<std::string>& value) { return value ? *value : std::string("unknown"); }

/// @return An optional number as an int, or -1 for unknown.
int NumberOr(const std::optional<std::uint32_t>& value) { return value ? static_cast<int>(*value) : -1; }

/// Prints the machine facts.
void PrintMachine() {
    ews::SystemMachineInfoSource source;
    auto machine = source.Read();
    if (!machine.ok()) {
        std::printf("machine: %s (error %lu)\n", StatusWord(machine.status), machine.error);
        return;
    }
    const ews::MachineInfo& m = machine.value;
    std::string chassis = m.smbios.chassisType ? ews::ChassisWord(*m.smbios.chassisType) : "unknown";
    int chassisCode = m.smbios.chassisType ? static_cast<int>(*m.smbios.chassisType) : -1;
    std::printf("machine: maker=%s model=%s\n", TextOr(m.smbios.maker).c_str(), TextOr(m.smbios.model).c_str());
    std::printf("chassis: %s (type %d)\n", chassis.c_str(), chassisCode);
    std::printf("processor: vendor=%s brand=%s logical=%d build=%d\n", TextOr(m.processorVendor).c_str(),
                TextOr(m.processorBrand).c_str(), NumberOr(m.logicalProcessors), NumberOr(m.windowsBuild));
}

/// Prints one EMI channel: index, name, kind and the watts between two samples.
void PrintEmiChannel(const ews::EmiChannel& channel, const ews::EmiSample& first, const ews::EmiSample& second) {
    auto watts = ews::EmiPowerWatts(first, second);
    std::printf("emi channel %u.%u: name=%s kind=%s power=%s %.3f W\n", channel.deviceIndex, channel.channelIndex,
                channel.name.c_str(), ews::EmiKindWord(channel.kind), StatusWord(watts.status), watts.value);
}

/// Prints every EMI channel with the power measured over 2 seconds.
void PrintEmi() {
    ews::SystemEmiSource source;
    auto channels = source.Open();
    if (!channels.ok()) {
        std::printf("emi: %s (error %lu)\n", StatusWord(channels.status), channels.error);
        return;
    }
    std::printf("emi channel count: %zu\n", channels.value.size());
    auto first = source.Read();
    Sleep(2000);  // two seconds between the two samples
    auto second = source.Read();
    bool sameSize = first.ok() && second.ok() && first.value.size() == channels.value.size() && second.value.size() == first.value.size();
    if (!sameSize) {
        std::printf("emi: read failed (%s, error %lu)\n", StatusWord(second.status), second.error);
        return;
    }
    for (std::size_t i = 0; i < channels.value.size(); ++i) {
        PrintEmiChannel(channels.value[i], first.value[i], second.value[i]);
    }
}

/// Prints one power meter instance.
void PrintPowerMeterInstance(const ews::PowerMeterInstance& instance) {
    std::printf("power meter instance: name=%s value=%.3f\n", instance.name.c_str(), instance.value);
}

/// Prints the ACPI power meter instances, or the status.
void PrintPowerMeter() {
    ews::SystemPowerMeterSource source;
    auto instances = source.Read();
    std::printf("power meter: %s (error 0x%lX) instances=%zu\n", StatusWord(instances.status), instances.error, instances.value.size());
    for (const ews::PowerMeterInstance& instance : instances.value) {
        PrintPowerMeterInstance(instance);
    }
}

/// Prints the display brightness for the current power source.
void PrintDisplay() {
    ews::SystemDisplaySource source;
    auto levels = source.Read();
    if (!levels.ok()) {
        std::printf("display: %s (error %lu)\n", StatusWord(levels.status), levels.error);
        return;
    }
    auto onBattery = ews::ReadOnBattery();
    auto percent = ews::BrightnessPercent(levels.value, onBattery.ok() && onBattery.value);
    std::printf("display: %s mains=%u battery=%u current=%d %%\n", StatusWord(percent.status), levels.value.onMains,
                levels.value.onBattery, percent.value);
}

/// Prints one graphics adapter.
void PrintAdapter(std::size_t index, const ews::GpuAdapter& adapter) {
    std::printf("gpu adapter %zu: %s vendor=0x%04X memory=%llu MB software=%d\n", index, adapter.description.c_str(), adapter.vendorId,
                static_cast<unsigned long long>(adapter.dedicatedMemoryMegabytes), adapter.software ? 1 : 0);
}

/// Prints one NVML device.
void PrintNvmlDevice(std::size_t index, const ews::NvmlPower& power) {
    std::printf("nvml device %zu: %s %u mW\n", index, power.known ? "ok" : "unknown", power.milliwatts);
}

/// Prints the graphics adapters and the NVML status.
void PrintGpu() {
    ews::SystemGpuSource source;
    auto adapters = source.ListAdapters();
    std::printf("gpu: %s (error 0x%lX) adapters=%zu\n", StatusWord(adapters.status), adapters.error, adapters.value.size());
    for (std::size_t i = 0; i < adapters.value.size(); ++i) {
        PrintAdapter(i, adapters.value[i]);
    }
    auto nvml = source.ReadNvmlPower();
    std::printf("nvml: %s (error %lu) devices=%zu\n", StatusWord(nvml.status), nvml.error, nvml.value.size());
    for (std::size_t i = 0; i < nvml.value.size(); ++i) {
        PrintNvmlDevice(i, nvml.value[i]);
    }
}

/// Prints whether the Intel DTT service exists.
void PrintIntelDtt() {
    ews::SystemIntelDttSource source;
    auto present = source.IsPresent();
    std::printf("intel dtt: %s present=%d (error %lu)\n", StatusWord(present.status), present.value ? 1 : 0, present.error);
}

}  // namespace

int RunLive() {
    PrintCpu();
    PrintBatteries();
    PrintOnBattery();
    PrintMachine();
    PrintEmi();
    PrintPowerMeter();
    PrintDisplay();
    PrintGpu();
    PrintIntelDtt();
    return 0;
}

}  // namespace ews_test
