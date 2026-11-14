// service_system.cpp: the real clock, power source, scan runner and sender factory of the service (see ews/service_core.h).
// Used by ews-service. Each class is a thin wrapper over a library function, so the core can be tested with fakes.
#include <chrono>

#include "ews/battery.h"
#include "ews/display.h"
#include "ews/emi.h"
#include "ews/gpu.h"
#include "ews/intel_dtt.h"
#include "ews/machine_info.h"
#include "ews/pawnio.h"
#include "ews/power_meter.h"
#include "ews/service_core.h"

namespace ews {

UtcTime SystemClock::Now() { return std::chrono::system_clock::now(); }

Reading<bool> SystemPowerSource::OnBattery() { return ReadOnBattery(); }

ScanResult SystemScanRunner::Run(ScanContext context) {
    SystemMachineInfoSource machine;
    SystemEmiSource emi;
    SystemBatterySource battery;
    SystemPowerMeterSource powerMeter;
    SystemDisplaySource display;
    SystemGpuSource gpu;
    SystemIntelDttSource intelDtt;
    SystemPawnIoSource pawnIo;
    SystemScanWaiter waiter;
    ScanSources sources{machine, emi, battery, powerMeter, display, gpu, intelDtt, pawnIo, waiter, ReadOnBattery};
    return RunScan(sources, context, FormatUtcTimestamp(std::chrono::system_clock::now()));
}

std::unique_ptr<ReportSender> WinHttpSenderFactory::Create(const std::string& url, const std::string& key) {
    return std::make_unique<WinHttpSender>(url, key);
}

const char* StartResultWord(StartResult result) {
    switch (result) {
        case StartResult::Ok: return "ok";
        case StartResult::DataFolderMissing: return "data_folder_missing";
        case StartResult::LockHeld: return "lock_held";
        case StartResult::LockFailed: return "lock_failed";
        case StartResult::IdentityFailed: return "identity_failed";
    }
    return "identity_failed";
}

}  // namespace ews
