// live.cpp: the --live mode of ews-core-tests. It reads this computer with the real readers and prints the values.
#include "live.h"

#include <windows.h>

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "ews/battery.h"
#include "ews/cpu_times.h"
#include "ews/machine_info.h"

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

}  // namespace

int RunLive() {
    PrintCpu();
    PrintBatteries();
    PrintOnBattery();
    PrintMachine();
    return 0;
}

}  // namespace ews_test
