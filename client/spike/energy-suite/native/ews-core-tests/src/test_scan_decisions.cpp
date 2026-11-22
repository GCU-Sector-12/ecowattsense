// test_scan_decisions.cpp: tests of the pure decision functions of SPEC 3.4 (level, calibration, device type).
#include "ews/scan.h"
#include "test_common.h"

using ews::Calibration;
using ews::DeviceType;
using ews::LevelFacts;
using ews::MeasurementLevel;

EWS_TEST(LevelL1ThroughLiveSystemChannel) {
    LevelFacts facts;
    facts.liveSystemChannel = true;
    CHECK(ews::DecideLevel(facts) == MeasurementLevel::L1);
}

EWS_TEST(LevelL2ThroughLiveCpuPackage) {
    LevelFacts facts;
    facts.liveCpuPackage = true;
    CHECK(ews::DecideLevel(facts) == MeasurementLevel::L2);
}

EWS_TEST(LevelL2IsNotGivenWithoutALivePackage) {
    LevelFacts facts;  // a package channel that exists but is not live leaves the flag false
    facts.mwhBattery = true;
    CHECK(ews::DecideLevel(facts) == MeasurementLevel::L3);
    facts.mwhBattery = false;
    CHECK(ews::DecideLevel(facts) == MeasurementLevel::L4);
}

EWS_TEST(LevelL3WithMwhBatteryAndNoLiveChannel) {
    LevelFacts facts;
    facts.mwhBattery = true;
    CHECK(ews::DecideLevel(facts) == MeasurementLevel::L3);
}

EWS_TEST(LevelL4WithoutBatteryOrWithRelativeBattery) {
    LevelFacts facts;  // a battery with a relative unit never sets mwhBattery
    CHECK(ews::DecideLevel(facts) == MeasurementLevel::L4);
}

EWS_TEST(LevelL1WinsOverL2) {
    LevelFacts facts;
    facts.liveSystemChannel = true;
    facts.liveCpuPackage = true;
    facts.mwhBattery = true;
    CHECK(ews::DecideLevel(facts) == MeasurementLevel::L1);
}

EWS_TEST(CalibrationNotNeededForL1) {
    CHECK(ews::DecideCalibration(MeasurementLevel::L1, true) == Calibration::NotNeeded);
    CHECK(ews::DecideCalibration(MeasurementLevel::L1, false) == Calibration::NotNeeded);
}

EWS_TEST(CalibrationNeededForL2AndL3WithMwhBattery) {
    CHECK(ews::DecideCalibration(MeasurementLevel::L2, true) == Calibration::Needed);
    CHECK(ews::DecideCalibration(MeasurementLevel::L3, true) == Calibration::Needed);
}

EWS_TEST(CalibrationNotPossibleWithoutBatteryAndForL4) {
    CHECK(ews::DecideCalibration(MeasurementLevel::L2, false) == Calibration::NotPossible);
    CHECK(ews::DecideCalibration(MeasurementLevel::L3, false) == Calibration::NotPossible);
    CHECK(ews::DecideCalibration(MeasurementLevel::L4, false) == Calibration::NotPossible);
    CHECK(ews::DecideCalibration(MeasurementLevel::L4, true) == Calibration::NotPossible);
}

EWS_TEST(DeviceTypeFromChassis) {
    CHECK(ews::DecideDeviceType(10, false) == DeviceType::Laptop);  // 10 = notebook
    CHECK(ews::DecideDeviceType(3, false) == DeviceType::Desktop);  // 3 = desktop
}

EWS_TEST(DeviceTypeOtherChassisWithBatteryIsLaptop) {
    CHECK(ews::DecideDeviceType(2, true) == DeviceType::Laptop);  // 2 = "unknown" chassis word "other"
    CHECK(ews::DecideDeviceType(2, false) == DeviceType::Desktop);
}

EWS_TEST(DeviceTypeUnknownChassisDecidesByBattery) {
    CHECK(ews::DecideDeviceType(std::nullopt, false) == DeviceType::Desktop);
    CHECK(ews::DecideDeviceType(std::nullopt, true) == DeviceType::Laptop);
}

EWS_TEST(DeviceTypeDesktopChassisWithBatteryIsLaptop) {
    CHECK(ews::DecideDeviceType(3, true) == DeviceType::Laptop);
}
