// test_scan.cpp: tests of RunScan with fake sources (no real waiting, no Windows calls).
#include <string>
#include <vector>

#include "ews/scan.h"
#include "fake_sources.h"
#include "test_common.h"

using ews::Calibration;
using ews::DeviceType;
using ews::MeasurementLevel;
using ews::Status;
using ews::Tri;
using ews_test::Scenario;

namespace {

/// @return Channel number `position` of the result, or an empty channel when there are fewer (so a red run does not crash).
const ews::EmiChannelResult& Channel(const ews::ScanResult& result, std::size_t position) {
    static const ews::EmiChannelResult empty;
    return position < result.emi.channels.size() ? result.emi.channels[position] : empty;
}

/// Runs the scan of a scenario.
ews::ScanResult Run(Scenario& scenario) { return ews::RunScan(scenario.Sources(), ews::ScanContext::User, "2026-11-14T10:00:00Z"); }

/// @return A failed reading of any list type with the given status and code.
template <typename T>
ews::Reading<T> Fail(Status status, unsigned long error) {
    return ews::Reading<T>::Failure(status, error);
}

}  // namespace

EWS_TEST(ScanHpScenarioGivesL2) {
    Scenario s;
    auto r = Run(s);
    CHECK(r.scannedUtc == "2026-11-14T10:00:00Z");
    CHECK(r.emi.common.present == Tri::True);
    CHECK(r.emi.common.readable == Tri::True);
    CHECK(r.emi.common.live == Tri::True);
    CHECK(r.emi.channels.size() == 4);
    CHECK(Channel(r, 0).kind == ews::EmiKind::CpuPackage);
    CHECK(Channel(r, 0).live == Tri::True);
    CHECK(Channel(r, 0).sampleWatts.has_value());
    CHECK(Channel(r, 0).sampleWatts.value_or(0.0) > 3.0);
    CHECK(Channel(r, 0).sampleWatts.value_or(100.0) < 5.0);
    CHECK(r.level == MeasurementLevel::L2);
    CHECK(r.calibration == Calibration::Needed);
    CHECK(r.deviceType == DeviceType::Laptop);
    CHECK(s.waiter.calls == 1);
}

EWS_TEST(ScanBatteryFields) {
    Scenario s;
    auto r = Run(s);
    CHECK(r.battery.common.present == Tri::True);
    CHECK(r.battery.common.readable == Tri::True);
    CHECK(r.battery.common.live == Tri::Unknown);
    CHECK(r.battery.count.value_or(0) == 1);
    CHECK(r.battery.unit.has_value());
    CHECK(r.battery.unit.value_or(ews::BatteryUnit::Relative) == ews::BatteryUnit::MilliwattHours);
    CHECK(r.battery.rateKnown.value_or(true) == false);
    CHECK(r.battery.fullMwh.value_or(0) == 36092);
    CHECK(r.battery.reportsMwh);
}

EWS_TEST(ScanPowerMeterPresentButNotLive) {
    Scenario s;
    auto r = Run(s);
    CHECK(r.powerMeter.common.present == Tri::True);
    CHECK(r.powerMeter.common.readable == Tri::True);
    CHECK(r.powerMeter.common.live == Tri::False);
    CHECK(r.powerMeter.instances.value_or(0) == 1);
}

EWS_TEST(ScanDisplayDeniedKeepsTheErrorCode) {
    Scenario s;
    auto r = Run(s);
    CHECK(r.display.common.present == Tri::True);
    CHECK(r.display.common.readable == Tri::False);
    CHECK(r.display.common.error == 5);
    CHECK(r.display.common.live == Tri::Unknown);
    CHECK(!r.display.brightnessPercent.has_value());
}

EWS_TEST(ScanDeniedSourceDoesNotStopTheOthers) {
    Scenario s;
    s.emi.open = Fail<std::vector<ews::EmiChannel>>(Status::Denied, 5);
    auto r = Run(s);
    CHECK(r.emi.common.present == Tri::True);
    CHECK(r.emi.common.readable == Tri::False);
    CHECK(r.emi.common.error == 5);
    CHECK(r.battery.common.readable == Tri::True);
    CHECK(r.powerMeter.common.readable == Tri::True);
    CHECK(r.gpu.common.readable == Tri::True);
    CHECK(r.level == MeasurementLevel::L3);
}

EWS_TEST(ScanUnavailableSourceIsNotPresent) {
    Scenario s;
    s.battery.result = Fail<std::vector<ews::BatteryState>>(Status::Unavailable, 2);
    s.powerMeter.first = Fail<std::vector<ews::PowerMeterInstance>>(Status::Unavailable, 0);
    s.powerMeter.second = Fail<std::vector<ews::PowerMeterInstance>>(Status::Unavailable, 0);
    s.dtt.result = Fail<bool>(Status::Unavailable, 1060);
    auto r = Run(s);
    CHECK(r.battery.common.present == Tri::False);
    CHECK(r.battery.common.readable == Tri::False);
    CHECK(r.powerMeter.common.present == Tri::False);
    CHECK(r.intelDtt.common.present == Tri::False);
}

EWS_TEST(ScanEmptyListsAreNotPresent) {
    Scenario s;
    s.emi.open = ews::Reading<std::vector<ews::EmiChannel>>::Success({});
    s.battery.result = ews::Reading<std::vector<ews::BatteryState>>::Success({});
    auto r = Run(s);
    CHECK(r.emi.common.present == Tri::False);
    CHECK(r.battery.common.present == Tri::False);
    CHECK(r.level == MeasurementLevel::L4);
    CHECK(r.calibration == Calibration::NotPossible);
}

EWS_TEST(ScanPackageChannelThatDoesNotGrowIsNotL2) {
    Scenario s;
    ews_test::SetEmiChannels(s, {"RAPL_Package0_PKG", "RAPL_Package0_DRAM"}, {false, true});
    auto r = Run(s);
    CHECK(Channel(r, 0).live == Tri::False);
    CHECK(Channel(r, 1).live == Tri::True);
    CHECK(r.emi.common.live == Tri::True);
    CHECK(r.level == MeasurementLevel::L3);
    CHECK(r.calibration == Calibration::Needed);
}

EWS_TEST(ScanLiveSystemChannelGivesL1) {
    Scenario s;
    ews_test::SetEmiChannels(s, {"PSYS"}, {true});
    auto r = Run(s);
    CHECK(Channel(r, 0).kind == ews::EmiKind::System);
    CHECK(r.level == MeasurementLevel::L1);
    CHECK(r.calibration == Calibration::NotNeeded);
}

EWS_TEST(ScanLivePowerMeterWithEmiPackageGivesL2NotL1) {
    Scenario s;  // the default scenario has EMI package and DRAM channels, all live
    s.powerMeter.second = ews::Reading<std::vector<ews::PowerMeterInstance>>::Success({{"System", 12.5}});
    auto r = Run(s);
    CHECK(r.powerMeter.common.live == Tri::True);
    CHECK(r.powerMeter.common.sample.value_or(0.0) == 12.5);
    CHECK(r.level == MeasurementLevel::L2);
    CHECK(r.calibration == Calibration::Needed);
}

EWS_TEST(ScanLivePowerMeterAloneDoesNotGiveL1) {
    Scenario s;  // no live EMI channel, an mWh battery: the meter must not lift the level above L3
    ews_test::SetEmiChannels(s, {"RAPL_Package0_PKG"}, {false});
    s.powerMeter.second = ews::Reading<std::vector<ews::PowerMeterInstance>>::Success({{"System", 12.5}});
    auto r = Run(s);
    CHECK(r.powerMeter.common.live == Tri::True);
    CHECK(r.level == MeasurementLevel::L3);
}

EWS_TEST(ScanPowerMeterLiveWhenOnlyTheFirstReadIsAboveZero) {
    Scenario s;
    s.powerMeter.first = ews::Reading<std::vector<ews::PowerMeterInstance>>::Success({{"System", 3.0}});
    auto r = Run(s);
    CHECK(r.powerMeter.common.live == Tri::True);
}

EWS_TEST(ScanBatteryWithRelativeUnitOnlyGivesL4) {
    Scenario s;
    ews::BatteryState relative;
    relative.unit = ews::BatteryUnit::Relative;
    relative.fullChargedKnown = true;
    relative.fullCharged = 100;
    s.battery.result = ews::Reading<std::vector<ews::BatteryState>>::Success({relative});
    ews_test::SetEmiChannels(s, {}, {});
    auto r = Run(s);
    CHECK(!r.battery.reportsMwh);
    CHECK(r.level == MeasurementLevel::L4);
    CHECK(r.calibration == Calibration::NotPossible);
    CHECK(r.deviceType == DeviceType::Laptop);
}

EWS_TEST(ScanMwhBatteryWithUnknownFullChargeDoesNotCount) {
    Scenario s;
    s.battery.result = ews::Reading<std::vector<ews::BatteryState>>::Success({ews_test::MakeMwhBattery(false)});
    ews_test::SetEmiChannels(s, {}, {});
    auto r = Run(s);
    CHECK(!r.battery.reportsMwh);
    CHECK(!r.battery.fullMwh.has_value());
    CHECK(r.level == MeasurementLevel::L4);
}

EWS_TEST(ScanCounterThatWentBackIsUnknown) {
    Scenario s;
    ews_test::SetEmiChannels(s, {"RAPL_Package0_PKG"}, {true});
    s.emi.second.value[0].energyPicowattHours = 5;  // far below the first read
    auto r = Run(s);
    CHECK(Channel(r, 0).live == Tri::Unknown);
    CHECK(!Channel(r, 0).sampleWatts.has_value());
    CHECK(r.emi.common.live == Tri::Unknown);
}

EWS_TEST(ScanSecondReadFailsLeavesLiveUnknown) {
    Scenario s;
    s.emi.second = Fail<std::vector<ews::EmiSample>>(Status::Error, 31);
    auto r = Run(s);
    CHECK(r.emi.common.readable == Tri::True);
    CHECK(r.emi.common.live == Tri::Unknown);
    CHECK(Channel(r, 0).live == Tri::Unknown);
}

EWS_TEST(ScanMachineUnknownLeavesFieldsEmptyAndDecidesByBattery) {
    Scenario s;
    s.machine.result = Fail<ews::MachineInfo>(Status::Error, 1);
    auto r = Run(s);
    CHECK(!r.machine.smbios.maker.has_value());
    CHECK(!r.machine.smbios.chassisType.has_value());
    CHECK(!r.machine.logicalProcessors.has_value());
    CHECK(r.deviceType == DeviceType::Laptop);
}

EWS_TEST(ScanEverySourceFailsStillFinishes) {
    Scenario s;
    s.machine.result = Fail<ews::MachineInfo>(Status::Error, 1);
    s.emi.open = Fail<std::vector<ews::EmiChannel>>(Status::Error, 2);
    s.battery.result = Fail<std::vector<ews::BatteryState>>(Status::Error, 3);
    s.powerMeter.first = Fail<std::vector<ews::PowerMeterInstance>>(Status::Error, 4);
    s.powerMeter.second = Fail<std::vector<ews::PowerMeterInstance>>(Status::Error, 4);
    s.display.result = Fail<ews::DisplayBrightness>(Status::Error, 6);
    s.gpu.adapters = Fail<std::vector<ews::GpuAdapter>>(Status::Error, 7);
    s.gpu.nvml = Fail<std::vector<ews::NvmlPower>>(Status::Error, 8);
    s.dtt.result = Fail<bool>(Status::Error, 9);
    auto r = Run(s);
    CHECK(s.waiter.calls == 1);
    CHECK(r.emi.common.error == 2);
    CHECK(r.battery.common.error == 3);
    CHECK(r.powerMeter.common.error == 4);
    CHECK(r.display.common.error == 6);
    CHECK(r.gpu.common.error == 7);
    CHECK(r.intelDtt.common.error == 9);
    CHECK(r.emi.common.present == Tri::Unknown);
    CHECK(r.emi.common.readable == Tri::False);
    CHECK(r.level == MeasurementLevel::L4);
    CHECK(r.calibration == Calibration::NotPossible);
    CHECK(r.deviceType == DeviceType::Desktop);
}

EWS_TEST(ScanDisplayReadableGivesBrightnessOfTheActivePowerSource) {
    Scenario s;
    s.display.result = ews::Reading<ews::DisplayBrightness>::Success(ews::DisplayBrightness{80, 40});
    auto r = Run(s);
    CHECK(r.display.common.readable == Tri::True);
    CHECK(r.display.brightnessPercent.value_or(-1) == 80);  // the fake reports mains
}

EWS_TEST(ScanGpuAndNvmlWords) {
    Scenario s;
    auto r = Run(s);
    CHECK(r.gpu.common.present == Tri::True);
    CHECK(r.gpu.adapters.size() == 1);
    CHECK(r.gpu.nvml.has_value());
    CHECK(r.gpu.nvml.value_or(ews::NvmlState::Ok) == ews::NvmlState::Unavailable);
    s.gpu.nvml = ews::Reading<std::vector<ews::NvmlPower>>::Success({ews::NvmlPower{true, 25000}});
    auto withNvml = Run(s);
    CHECK(withNvml.gpu.nvml.value_or(ews::NvmlState::Error) == ews::NvmlState::Ok);
    CHECK(withNvml.gpu.nvmlDevices.value_or(0) == 1);
}

EWS_TEST(ScanContextIsRecorded) {
    Scenario s;
    auto r = ews::RunScan(s.Sources(), ews::ScanContext::Service, "t");
    CHECK(r.context == ews::ScanContext::Service);
}
