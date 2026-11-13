// test_energy.cpp: tests of the energy of one minute (ews/energy.h, SPEC 4.2 and 4.5).
// Every expected number is calculated by hand in the comment above the check (model of SPEC section 8).
#include "ews/energy.h"
#include "test_calc_common.h"
#include "test_common.h"

using ews::CalculateMinuteEnergy;
using ews::MeasureLevel;
using ews::MinuteEnergy;
using ews::MinuteInput;
using ews_test::Near;

namespace {

/// @return A 60 second minute at 20 % processor with the display on at 80 %, no EMI growth, the given model.
MinuteInput BaseMinute(const ews::CalibrationModel* model) {
    MinuteInput in;
    in.seconds = 60;
    in.cpuPercent = 20.0;
    in.displayOn = true;
    in.brightnessPct = 80.0;
    in.model = model;
    return in;
}

}  // namespace

EWS_TEST(EnergyL1FromSystemGrowth) {
    MinuteInput in = BaseMinute(nullptr);
    in.systemGrowthUj = 300e6;
    MinuteEnergy e = CalculateMinuteEnergy(in);
    CHECK(e.level == MeasureLevel::L1);
    CHECK(e.deviceUj.has_value() && Near(*e.deviceUj, 300e6));
    CHECK(Near(e.measuredUj, 300e6));
}

EWS_TEST(EnergyL2WithDisplayOn) {
    // rest 2.24 + 0.009 x 20 = 2.42 W; screen 1.40 + (80 - 50) x 0.0104 = 1.712 W; 4.132 W x 60 s = 247.92 J.
    // measured 120 000 000 + 30 000 000 = 150 000 000 uJ; device 150 000 000 + 247 920 000 = 397 920 000 uJ.
    ews::CalibrationModel model = ews_test::SpecModel();
    MinuteInput in = BaseMinute(&model);
    in.packageGrowthUj = 120e6;
    in.dramGrowthUj = 30e6;
    MinuteEnergy e = CalculateMinuteEnergy(in);
    CHECK(e.level == MeasureLevel::L2);
    CHECK(e.deviceUj.has_value() && Near(*e.deviceUj, 397920000.0));
    CHECK(Near(e.measuredUj, 150e6));
}

EWS_TEST(EnergyL2WithDisplayOff) {
    // rest 2.42 W x 60 s = 145.2 J = 145 200 000 uJ; device 150 000 000 + 145 200 000 = 295 200 000 uJ.
    ews::CalibrationModel model = ews_test::SpecModel();
    MinuteInput in = BaseMinute(&model);
    in.displayOn = false;
    in.packageGrowthUj = 120e6;
    in.dramGrowthUj = 30e6;
    MinuteEnergy e = CalculateMinuteEnergy(in);
    CHECK(e.level == MeasureLevel::L2);
    CHECK(e.deviceUj.has_value() && Near(*e.deviceUj, 295200000.0));
}

EWS_TEST(EnergyL2OnlyPackageNamed) {
    // measured_parts holds only the package, so the dram growth is not needed: 120 000 000 + 247 920 000.
    ews::CalibrationModel model = ews_test::SpecModel();
    model.measuresDram = false;
    MinuteInput in = BaseMinute(&model);
    in.packageGrowthUj = 120e6;
    MinuteEnergy e = CalculateMinuteEnergy(in);
    CHECK(e.level == MeasureLevel::L2);
    CHECK(e.deviceUj.has_value() && Near(*e.deviceUj, 367920000.0));
    CHECK(Near(e.measuredUj, 120e6));
}

EWS_TEST(EnergyL2FallsToL3WhenDramUnknown) {
    // L3: base 3.48 + 0.075 x 20 = 4.98 W; screen 1.712 W; 6.692 W x 60 s = 401.52 J = 401 520 000 uJ.
    ews::CalibrationModel model = ews_test::SpecModel();
    MinuteInput in = BaseMinute(&model);
    in.packageGrowthUj = 120e6;
    MinuteEnergy e = CalculateMinuteEnergy(in);
    CHECK(e.level == MeasureLevel::L3);
    CHECK(e.deviceUj.has_value() && Near(*e.deviceUj, 401520000.0));
    CHECK(Near(e.measuredUj, 0.0));
}

EWS_TEST(EnergyL3WithDisplayOff) {
    // (3.48 + 1.5) W x 60 s = 298.8 J = 298 800 000 uJ. The model names no measured part.
    ews::CalibrationModel model = ews_test::SpecModel();
    model.measuresCpuPackage = false;
    model.measuresDram = false;
    MinuteInput in = BaseMinute(&model);
    in.displayOn = false;
    MinuteEnergy e = CalculateMinuteEnergy(in);
    CHECK(e.level == MeasureLevel::L3);
    CHECK(e.deviceUj.has_value() && Near(*e.deviceUj, 298800000.0));
}

EWS_TEST(EnergyL3UnknownWhenProcessorUnknown) {
    ews::CalibrationModel model = ews_test::SpecModel();
    MinuteInput in = BaseMinute(&model);
    in.cpuPercent.reset();
    MinuteEnergy e = CalculateMinuteEnergy(in);
    CHECK(e.level == MeasureLevel::Unknown);
    CHECK(!e.deviceUj.has_value());
}

EWS_TEST(EnergyUnknownWithoutModelAndSystem) {
    MinuteInput in = BaseMinute(nullptr);
    in.packageGrowthUj = 120e6;
    in.dramGrowthUj = 30e6;
    MinuteEnergy e = CalculateMinuteEnergy(in);
    CHECK(e.level == MeasureLevel::Unknown);
    CHECK(!e.deviceUj.has_value());
}

EWS_TEST(EnergyUnknownBrightnessCountsAs50) {
    // L3, 0 % processor, display on: (3.48 + 1.40) W x 60 s = 292.8 J = 292 800 000 uJ.
    ews::CalibrationModel model = ews_test::SpecModel();
    MinuteInput in = BaseMinute(&model);
    in.cpuPercent = 0.0;
    in.brightnessPct.reset();
    MinuteEnergy e = CalculateMinuteEnergy(in);
    CHECK(e.level == MeasureLevel::L3);
    CHECK(e.deviceUj.has_value() && Near(*e.deviceUj, 292800000.0));
}

EWS_TEST(EnergyScreenPowerNeverBelowZero) {
    // screen 1.0 + (0 - 50) x 0.1 = -4 W, which counts as 0: 2.0 W x 60 s = 120 000 000 uJ.
    ews::CalibrationModel model = ews_test::SpecModel();
    model.measuresCpuPackage = false;
    model.measuresDram = false;
    model.baseW = 2.0;
    model.cpuWPerPct = 0.0;
    model.screenWAt50 = 1.0;
    model.screenWPerPct = 0.1;
    MinuteInput in = BaseMinute(&model);
    in.brightnessPct = 0.0;
    MinuteEnergy e = CalculateMinuteEnergy(in);
    CHECK(e.deviceUj.has_value() && Near(*e.deviceUj, 120e6));
}

EWS_TEST(EnergyBadInputsAreUnknown) {
    ews::CalibrationModel model = ews_test::SpecModel();
    MinuteInput zero = BaseMinute(&model);
    zero.seconds = 0;
    CHECK(CalculateMinuteEnergy(zero).level == MeasureLevel::Unknown);
    MinuteInput negativeSystem = BaseMinute(nullptr);
    negativeSystem.systemGrowthUj = -5.0;
    CHECK(CalculateMinuteEnergy(negativeSystem).level == MeasureLevel::Unknown);
    MinuteInput negativeFallsBack = BaseMinute(&model);
    negativeFallsBack.systemGrowthUj = -5.0;
    CHECK(CalculateMinuteEnergy(negativeFallsBack).level == MeasureLevel::L3);
    MinuteInput percentTooHigh = BaseMinute(&model);
    percentTooHigh.cpuPercent = 101.0;
    CHECK(CalculateMinuteEnergy(percentTooHigh).level == MeasureLevel::Unknown);
}

EWS_TEST(EnergyLevelWords) {
    CHECK(std::string(ews::MeasureLevelWord(MeasureLevel::L1)) == "L1");
    CHECK(std::string(ews::MeasureLevelWord(MeasureLevel::L2)) == "L2");
    CHECK(std::string(ews::MeasureLevelWord(MeasureLevel::L3)) == "L3");
    CHECK(std::string(ews::MeasureLevelWord(MeasureLevel::Unknown)).empty());
}
