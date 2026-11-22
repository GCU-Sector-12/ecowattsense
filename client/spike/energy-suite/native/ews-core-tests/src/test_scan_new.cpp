// test_scan_new.cpp: tests of the scanner additions for AMD laptops and extra data (SPEC 1.1 to 1.5): cpu_core channels,
// machine, battery, display and graphics fields, the source gpu_use, and the JSON text of all of them.
#include <string>
#include <vector>

#include "ews/scan.h"
#include "ews/scan_output.h"
#include "fake_sources.h"
#include "test_common.h"

using ews::Status;
using ews::Tri;
using ews_test::Scenario;

namespace {

/// Runs the scan of a scenario.
ews::ScanResult Run(Scenario& scenario) { return ews::RunScan(scenario.Sources(), ews::ScanContext::User, "2026-11-14T10:00:00Z"); }

/// @return True when `text` holds `part`.
bool Has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

/// Builds a battery with the given designed capacity and cycle count.
ews::BatteryState Battery(ews::BatteryUnit unit, bool designedKnown, std::uint32_t designed, std::uint32_t cycles) {
    ews::BatteryState battery;
    battery.unit = unit;
    battery.designedKnown = designedKnown;
    battery.designed = designed;
    battery.cycleCount = cycles;
    return battery;
}

/// Builds an adapter with a LUID.
ews::GpuAdapter Adapter(std::uint32_t vendor, std::uint32_t device, std::uint64_t luid) {
    ews::GpuAdapter adapter;
    adapter.description = "Test";
    adapter.vendorId = vendor;
    adapter.deviceId = device;
    adapter.luid = luid;
    return adapter;
}

/// Builds one GPU Engine instance name and value.
ews::PdhInstance Engine(const std::string& luidText, int engine, const std::string& type, double value) {
    return ews::PdhInstance{"pid_4_luid_" + luidText + "_phys_0_eng_" + std::to_string(engine) + "_engtype_" + type, value};
}

}  // namespace

// ---- 1.1 cpu_core ----

EWS_TEST(ScanAmdLaptopWithCoreChannelsGivesL2FromThePackage) {
    Scenario s;
    ews_test::SetEmiChannels(s, {"RAPL_Package0_PKG", "RAPL_Package0_Core0_CORE", "RAPL_Package0_Core2_CORE", "RAPL_Package0_Core4_CORE"},
                             {true, true, true, true});
    auto r = Run(s);
    CHECK(r.level == ews::MeasurementLevel::L2);
    CHECK(r.emi.channels.size() == 4);
    CHECK(r.emi.channels.size() == 4 && r.emi.channels[1].kind == ews::EmiKind::CpuCore);
    CHECK(r.emi.channels.size() == 4 && r.emi.channels[3].kind == ews::EmiKind::CpuCore);
}

EWS_TEST(ScanLiveCoreChannelsAloneNeverDecideALevel) {
    Scenario s;
    ews_test::SetEmiChannels(s, {"RAPL_Package0_Core0_CORE", "RAPL_Package0_Core2_CORE"}, {true, true});
    s.battery.result = ews::Reading<std::vector<ews::BatteryState>>::Success({});
    auto r = Run(s);
    CHECK(r.emi.common.live == Tri::True);
    CHECK(r.level == ews::MeasurementLevel::L4);
}

EWS_TEST(ScanOtherAmdChannelNamesStayOther) {
    Scenario s;
    ews_test::SetEmiChannels(s, {"RAPL_Package0_PKG", "VDDCR_VDD Energy", "Apu Energy"}, {true, true, true});
    auto r = Run(s);
    CHECK(r.emi.channels.size() == 3);
    CHECK(r.emi.channels.size() == 3 && r.emi.channels[1].kind == ews::EmiKind::Other);
    CHECK(r.emi.channels.size() == 3 && r.emi.channels[2].kind == ews::EmiKind::Other);
    CHECK(r.level == ews::MeasurementLevel::L2);
}

EWS_TEST(JsonShowsTheCpuCoreKind) {
    Scenario s;
    ews_test::SetEmiChannels(s, {"RAPL_Package0_PKG", "RAPL_Package0_Core0_CORE"}, {true, true});
    auto text = ews::WriteCapabilitiesJson(Run(s));
    CHECK(Has(text, "\"name\": \"RAPL_Package0_Core0_CORE\", \"kind\": \"cpu_core\""));
}

// ---- 1.2 machine ----

EWS_TEST(JsonMachineHasTheProcessorFacts) {
    ews::MachineInfo info;
    info.smbios.maker = "HP";
    info.processorVendor = "AuthenticAMD";
    info.processorBrand = "AMD Ryzen 7";
    info.cpuFamily = 0x17;
    info.cpuModel = 0x60;
    info.cpuStepping = 1;
    info.logicalProcessors = 16;
    info.physicalCores = 8;
    info.windowsBuild = 26300;
    CHECK(ews::MachineObjectJson(info) ==
          "{ \"maker\": \"HP\", \"cpu_vendor\": \"AuthenticAMD\", \"cpu_name\": \"AMD Ryzen 7\", \"cpu_family\": 23, "
          "\"cpu_model\": 96, \"cpu_stepping\": 1, \"logical_processors\": 16, \"physical_cores\": 8, \"windows_build\": 26300 }");
}

EWS_TEST(JsonMachineLeavesOutUnknownProcessorFacts) {
    ews::MachineInfo info;
    info.logicalProcessors = 4;
    CHECK(ews::MachineObjectJson(info) == "{ \"logical_processors\": 4 }");
}

// ---- 1.3 battery ----

EWS_TEST(ScanBatterySumsTheDesignedCapacityAndTakesTheLargestCycleCount) {
    Scenario s;
    s.battery.result = ews::Reading<std::vector<ews::BatteryState>>::Success(
        {Battery(ews::BatteryUnit::MilliwattHours, true, 40000, 120), Battery(ews::BatteryUnit::MilliwattHours, true, 10000, 300)});
    auto r = Run(s);
    CHECK(r.battery.designMwh.value_or(0) == 50000);
    CHECK(r.battery.cycleCount.value_or(0) == 300);
}

EWS_TEST(ScanBatteryDesignedCapacityCountsOnlyKnownMwhBatteries) {
    Scenario s;
    s.battery.result = ews::Reading<std::vector<ews::BatteryState>>::Success(
        {Battery(ews::BatteryUnit::MilliwattHours, false, 40000, 0), Battery(ews::BatteryUnit::Relative, true, 100, 0)});
    auto r = Run(s);
    CHECK(!r.battery.designMwh.has_value());
    CHECK(!r.battery.cycleCount.has_value());  // 0 means unknown
}

EWS_TEST(ScanBatteryFailedReadLeavesTheNewFieldsOut) {
    Scenario s;
    s.battery.result = ews::Reading<std::vector<ews::BatteryState>>::Failure(Status::Error, 3);
    auto r = Run(s);
    CHECK(!r.battery.designMwh.has_value());
    CHECK(!r.battery.cycleCount.has_value());
}

EWS_TEST(JsonBatteryHasDesignAndCycles) {
    Scenario s;
    auto battery = Battery(ews::BatteryUnit::MilliwattHours, true, 40000, 250);
    battery.fullChargedKnown = true;
    battery.fullCharged = 36000;
    s.battery.result = ews::Reading<std::vector<ews::BatteryState>>::Success({battery});
    auto text = ews::BatteryObjectJson(Run(s).battery);
    CHECK(Has(text, "\"full_mwh\": 36000, \"design_mwh\": 40000, \"cycle_count\": 250 }"));
}

// ---- 1.4 display ----

EWS_TEST(ScanDisplayLayoutIsReadWhenTheBrightnessIsDenied) {
    Scenario s;  // the brightness is denied in the scenario
    ews::DisplayLayout layout;
    layout.displays = 2;
    layout.externalDisplays = 1;
    layout.refreshHz = 60;
    s.displayLayout.result = ews::Reading<ews::DisplayLayout>::Success(layout);
    auto r = Run(s);
    CHECK(r.display.common.readable == Tri::False);
    CHECK(r.display.displays.value_or(0) == 2);
    CHECK(r.display.externalDisplays.value_or(99) == 1);
    CHECK(r.display.refreshHz.value_or(0) == 60);
}

EWS_TEST(ScanDisplayLayoutFailureLeavesTheFieldsOut) {
    Scenario s;
    s.displayLayout.result = ews::Reading<ews::DisplayLayout>::Failure(Status::Denied, 5);
    auto r = Run(s);
    CHECK(!r.display.displays.has_value());
    CHECK(!r.display.externalDisplays.has_value());
    CHECK(!r.display.refreshHz.has_value());
}

EWS_TEST(JsonDisplayHasTheLayoutFields) {
    Scenario s;
    s.display.result = ews::Reading<ews::DisplayBrightness>::Success(ews::DisplayBrightness{80, 40});
    ews::DisplayLayout layout;
    layout.displays = 1;
    layout.externalDisplays = 0;
    layout.refreshHz = 144;
    s.displayLayout.result = ews::Reading<ews::DisplayLayout>::Success(layout);
    CHECK(ews::DisplayObjectJson(Run(s).display) ==
          "{ \"id\": \"display\", \"present\": true, \"readable\": true, \"live\": null, \"error\": 0, \"brightness_pct\": 80, "
          "\"displays\": 1, \"external_displays\": 0, \"refresh_hz\": 144 }");
}

// ---- 1.5 gpu ----

EWS_TEST(ScanGpuListsTheNvmlDevicesAndLinksThemToAdapters) {
    Scenario s;
    s.gpu.adapters = ews::Reading<std::vector<ews::GpuAdapter>>::Success({Adapter(0x8086, 0x9A49, 1), Adapter(0x10DE, 0x2520, 2)});
    ews::NvmlDeviceFacts facts;
    facts.name = "NVIDIA Test";
    facts.vendorId = 0x10DE;
    facts.deviceId = 0x2520;
    facts.powerAnswered = true;
    facts.energyAnswered = false;
    facts.milliwatts = 15300;
    s.gpu.devices = ews::Reading<std::vector<ews::NvmlDeviceFacts>>::Success({facts});
    auto r = Run(s);
    CHECK(r.gpu.nvmlEntries.size() == 1);
    CHECK(r.gpu.nvmlEntries.size() == 1 && r.gpu.nvmlEntries[0].name == "NVIDIA Test");
    CHECK(r.gpu.nvmlEntries.size() == 1 && r.gpu.nvmlEntries[0].adapter.value_or(99) == 1);
    CHECK(r.gpu.nvmlEntries.size() == 1 && r.gpu.nvmlEntries[0].power && !r.gpu.nvmlEntries[0].energy);
    CHECK(r.gpu.nvmlEntries.size() == 1 && r.gpu.nvmlEntries[0].sampleWatts.value_or(0) == 15.3);
}

EWS_TEST(ScanGpuWithoutNvmlHasNoNvmlEntries) {
    Scenario s;  // the devices read is Unavailable in the scenario
    CHECK(Run(s).gpu.nvmlEntries.empty());
}

EWS_TEST(ScanGpuADeviceWithoutAMatchingAdapterHasNoAdapterIndex) {
    Scenario s;
    ews::NvmlDeviceFacts facts;
    facts.name = "NVIDIA Test";
    facts.vendorId = 0x10DE;
    facts.deviceId = 0x2520;
    s.gpu.devices = ews::Reading<std::vector<ews::NvmlDeviceFacts>>::Success({facts});
    auto r = Run(s);
    CHECK(r.gpu.nvmlEntries.size() == 1);
    CHECK(r.gpu.nvmlEntries.size() == 1 && !r.gpu.nvmlEntries[0].adapter.has_value());
    CHECK(r.gpu.nvmlEntries.size() == 1 && !r.gpu.nvmlEntries[0].sampleWatts.has_value());
}

EWS_TEST(JsonNvmlEntries) {
    ews::NvmlEntry one;
    one.name = "GPU 1";
    one.adapter = 1;
    one.power = true;
    one.energy = true;
    one.sampleWatts = 15.3;
    ews::NvmlEntry two;
    two.name = "GPU 2";
    CHECK(ews::NvmlEntriesJson({one, two}) ==
          "[ { \"name\": \"GPU 1\", \"adapter\": 1, \"power\": true, \"energy\": true, \"sample_w\": 15.3 }, "
          "{ \"name\": \"GPU 2\", \"power\": false, \"energy\": false } ]");
    CHECK(ews::NvmlEntriesJson({}) == "[ ]");
}

EWS_TEST(JsonGpuHoldsTheNvmlListUnderNvmlList) {
    Scenario s;
    s.gpu.adapters = ews::Reading<std::vector<ews::GpuAdapter>>::Success({Adapter(0x8086, 0x9A49, 1), Adapter(0x10DE, 0x2520, 2)});
    s.gpu.nvml = ews::Reading<std::vector<ews::NvmlPower>>::Success({ews::NvmlPower{true, 15300}});
    ews::NvmlDeviceFacts facts;
    facts.name = "NVIDIA Test";
    facts.vendorId = 0x10DE;
    facts.deviceId = 0x2520;
    facts.powerAnswered = true;
    facts.milliwatts = 15300;
    s.gpu.devices = ews::Reading<std::vector<ews::NvmlDeviceFacts>>::Success({facts});
    auto text = ews::WriteCapabilitiesJson(Run(s));
    CHECK(Has(text, "\"nvml\": \"ok\", \"nvml_devices\": 1, \"nvml_list\": [ { \"name\": \"NVIDIA Test\", \"adapter\": 1, "
                    "\"power\": true, \"energy\": false, \"sample_w\": 15.3 } ] },"));
}

EWS_TEST(JsonGpuWithoutNvmlDevicesHasNoNvmlList) {
    Scenario s;
    CHECK(!Has(ews::WriteCapabilitiesJson(Run(s)), "nvml_list"));
}

EWS_TEST(JsonAdapterHasTheRole) {
    Scenario s;
    auto adapter = Adapter(0x8086, 0x9A49, 1);
    adapter.role = ews::GpuRole::PowerSaving;
    s.gpu.adapters = ews::Reading<std::vector<ews::GpuAdapter>>::Success({adapter, Adapter(0x10DE, 0x2520, 2)});
    auto text = ews::WriteCapabilitiesJson(Run(s));
    CHECK(Has(text, "\"software\": false, \"role\": \"power_saving\" }"));
    CHECK(Has(text, "{ \"name\": \"Test\", \"vendor_id\": 4318, \"memory_mb\": 0, \"software\": false } ]"));
}

EWS_TEST(JsonNeverHoldsTheAdapterLuid) {
    Scenario s;
    s.gpu.adapters = ews::Reading<std::vector<ews::GpuAdapter>>::Success({Adapter(0x8086, 0x9A49, 0xA1B2)});
    auto text = ews::WriteCapabilitiesJson(Run(s));
    CHECK(!Has(text, "luid"));
    CHECK(!Has(text, "41394"));  // 0xA1B2
}

// ---- 1.5 gpu_use ----

EWS_TEST(ScanGpuUseNeedsTheSecondReadAndKeepsLiveUnknown) {
    Scenario s;
    s.gpuUse.first = ews::Reading<std::vector<ews::PdhInstance>>::Failure(Status::Unavailable, 0);
    s.gpuUse.second = ews::Reading<std::vector<ews::PdhInstance>>::Success(
        {Engine("0x00000000_0x0000A1B2", 0, "3D", 12.5), Engine("0x00000000_0x0000C3D4", 0, "3D", 40.0),
         Engine("0x00000000_0x0000C3D4", 1, "Copy", 90.0)});
    auto r = Run(s);
    CHECK(r.gpuUse.common.present == Tri::True);
    CHECK(r.gpuUse.common.readable == Tri::True);
    CHECK(r.gpuUse.common.live == Tri::Unknown);
    CHECK(r.gpuUse.adapters.value_or(0) == 2);
    CHECK(r.gpuUse.common.sample.value_or(-1) == 40.0);  // the highest use of one adapter
}

EWS_TEST(ScanGpuUseFirstReadFailureIsOnlyThePrimingRead) {
    Scenario s;
    s.gpuUse.second = ews::Reading<std::vector<ews::PdhInstance>>::Success({Engine("0x0_0x1", 0, "3D", 5.0)});
    CHECK(Run(s).gpuUse.common.readable == Tri::True);
}

EWS_TEST(ScanGpuUseNoCounterSetMeansNotPresent) {
    Scenario s;  // both reads are Unavailable in the scenario
    auto r = Run(s);
    CHECK(r.gpuUse.common.present == Tri::False);
    CHECK(r.gpuUse.common.readable == Tri::False);
    CHECK(!r.gpuUse.adapters.has_value());
    CHECK(!r.gpuUse.common.sample.has_value());
}

EWS_TEST(ScanGpuUseAnEmptyListMeansNotPresent) {
    Scenario s;
    s.gpuUse.second = ews::Reading<std::vector<ews::PdhInstance>>::Success({});
    CHECK(Run(s).gpuUse.common.present == Tri::False);
}

EWS_TEST(ScanGpuUseAFailedReadIsUnknownNotMissing) {
    Scenario s;
    s.gpuUse.second = ews::Reading<std::vector<ews::PdhInstance>>::Failure(Status::Error, 77);
    auto r = Run(s);
    CHECK(r.gpuUse.common.present == Tri::Unknown);
    CHECK(r.gpuUse.common.readable == Tri::False);
    CHECK(r.gpuUse.common.error == 77);
}

EWS_TEST(ScanGpuUseInstancesWithoutAnyThreeDEngineGiveNoSample) {
    Scenario s;
    s.gpuUse.second = ews::Reading<std::vector<ews::PdhInstance>>::Success({Engine("0x0_0x1", 0, "Copy", 5.0)});
    auto r = Run(s);
    CHECK(r.gpuUse.common.readable == Tri::True);
    CHECK(r.gpuUse.adapters.value_or(0) == 1);
    CHECK(!r.gpuUse.common.sample.has_value());
}

EWS_TEST(JsonGpuUseSitsBetweenGpuAndIntelDtt) {
    Scenario s;
    s.gpuUse.second = ews::Reading<std::vector<ews::PdhInstance>>::Success({Engine("0x0_0x1", 0, "3D", 5.0)});
    auto text = ews::WriteCapabilitiesJson(Run(s));
    auto gpu = text.find("\"id\": \"gpu\"");
    auto use = text.find("\"id\": \"gpu_use\"");
    auto dtt = text.find("\"id\": \"intel_dtt\"");
    CHECK(gpu < use);
    CHECK(use < dtt);
    CHECK(Has(text, "{ \"id\": \"gpu_use\", \"present\": true, \"readable\": true, \"live\": null, \"error\": 0, \"adapters\": 1 }"));
}

EWS_TEST(ScanLogHasTheFourStepsOfGpuUse) {
    Scenario s;
    s.gpuUse.second = ews::Reading<std::vector<ews::PdhInstance>>::Success({Engine("0x0_0x1", 0, "3D", 5.0)});
    auto log = ews::WriteScanLog(Run(s));
    CHECK(Has(log, "gpu_use step=present result=yes"));
    CHECK(Has(log, "gpu_use step=readable result=yes"));
    CHECK(Has(log, "gpu_use step=live result=unknown"));
    CHECK(Has(log, "gpu_use step=sample result=ok"));
}
