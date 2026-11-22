// test_energy.cpp: tests of the energy of one minute (ews/energy.h, SPEC 4.2 and 4.5).
// Every expected number is calculated by hand in the comment above the check (model of SPEC section 8).
#include <cmath>
#include <limits>

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

namespace {

/// @return True when two watt values are the same to a nanowatt.
bool SameWatts(const std::optional<double>& value, double expected) { return value.has_value() && std::fabs(*value - expected) < 1e-9; }

}  // namespace

EWS_TEST(EnergyReportsTheWattTermsOfLevel2) {
    // rest 2.24 W; processor term 0.009 x 20 = 0.18 W; screen 1.40 + (80 - 50) x 0.0104 = 1.712 W.
    ews::CalibrationModel model = ews_test::SpecModel();
    MinuteInput in = BaseMinute(&model);
    in.packageGrowthUj = 120e6;
    in.dramGrowthUj = 30e6;
    MinuteEnergy e = CalculateMinuteEnergy(in);
    CHECK(e.level == MeasureLevel::L2);
    CHECK(!e.terms.baseW.has_value());
    CHECK(SameWatts(e.terms.restW, 2.24));
    CHECK(SameWatts(e.terms.cpuW, 0.18));
    CHECK(SameWatts(e.terms.screenW, 1.712));
}

EWS_TEST(EnergyReportsTheWattTermsOfLevel3) {
    // base 3.48 W; processor term 0.075 x 20 = 1.5 W; screen 1.712 W. Without EMI growth the minute is level 3.
    ews::CalibrationModel model = ews_test::SpecModel();
    MinuteInput in = BaseMinute(&model);
    MinuteEnergy e = CalculateMinuteEnergy(in);
    CHECK(e.level == MeasureLevel::L3);
    CHECK(SameWatts(e.terms.baseW, 3.48));
    CHECK(!e.terms.restW.has_value());
    CHECK(SameWatts(e.terms.cpuW, 1.5));
    CHECK(SameWatts(e.terms.screenW, 1.712));
}

EWS_TEST(EnergyHasNoWattTermsAtLevel1OrWhenUnknown) {
    MinuteInput in = BaseMinute(nullptr);
    in.systemGrowthUj = 300e6;
    MinuteEnergy l1 = CalculateMinuteEnergy(in);
    CHECK(!l1.terms.baseW.has_value() && !l1.terms.restW.has_value() && !l1.terms.cpuW.has_value() && !l1.terms.screenW.has_value());
    MinuteEnergy unknown = CalculateMinuteEnergy(BaseMinute(nullptr));
    CHECK(unknown.level == MeasureLevel::Unknown);
    CHECK(!unknown.terms.cpuW.has_value());
}

EWS_TEST(EnergyToWattsRule) {
    CHECK(SameWatts(ews::EnergyToWatts(4.0e6, 2.0), 2.0));
    CHECK(SameWatts(ews::EnergyToWatts(-8.2e6 * 60.0, 60.0), -8.2));
    CHECK(!ews::EnergyToWatts(std::nullopt, 60.0).has_value());
    CHECK(!ews::EnergyToWatts(1.0e6, 0.0).has_value());
    CHECK(!ews::EnergyToWatts(1.0e6, -5.0).has_value());
}

namespace {

/// @return The spec model with the two graphics numbers: rest_w_per_gpu_pct 0.03 (L2) and gpu_w_per_pct 0.04 (L3).
ews::CalibrationModel GraphicsModel() {
    ews::CalibrationModel model = ews_test::SpecModel();
    model.schema = 3;
    model.gpuWPerPct = 0.04;
    model.restWPerGpuPct = 0.03;
    return model;
}

/// @return True when two minutes are bit for bit the same: level, device energy, measured energy.
bool SameMinute(const MinuteEnergy& a, const MinuteEnergy& b) {
    return a.level == b.level && a.deviceUj == b.deviceUj && a.measuredUj == b.measuredUj;
}

}  // namespace

EWS_TEST(EnergyL2AddsRestWPerGpuPctTimesTheGpuUse) {
    // rest 2.42 W + screen 1.712 W + 0.03 x 50 = 1.5 W: 5.632 W x 60 s = 337.92 J; measured 150 000 000 uJ.
    ews::CalibrationModel model = GraphicsModel();
    MinuteInput in = BaseMinute(&model);
    in.packageGrowthUj = 120e6;
    in.dramGrowthUj = 30e6;
    in.gpuPercent = 50.0;
    MinuteEnergy e = CalculateMinuteEnergy(in);
    CHECK(e.level == MeasureLevel::L2);
    CHECK(e.deviceUj.has_value() && Near(*e.deviceUj, 487920000.0));
    CHECK(Near(e.measuredUj, 150e6));
    CHECK(e.terms.gpuW.has_value() && Near(*e.terms.gpuW * 1e6, 1.5e6));
}

EWS_TEST(EnergyL3AddsGpuWPerPctTimesTheGpuUse) {
    // base 3.48 + 0.075 x 20 = 4.98 W + screen 1.712 W + 0.04 x 50 = 2.0 W: 8.692 W x 60 s = 521.52 J.
    ews::CalibrationModel model = GraphicsModel();
    MinuteInput in = BaseMinute(&model);
    in.gpuPercent = 50.0;
    MinuteEnergy e = CalculateMinuteEnergy(in);
    CHECK(e.level == MeasureLevel::L3);
    CHECK(e.deviceUj.has_value() && Near(*e.deviceUj, 521520000.0));
    CHECK(Near(e.measuredUj, 0.0));
    CHECK(e.terms.gpuW.has_value() && Near(*e.terms.gpuW * 1e6, 2.0e6));
}

EWS_TEST(EnergyUnknownGpuUseGivesTheSameMinuteAsBefore) {
    ews::CalibrationModel withNumbers = GraphicsModel();
    ews::CalibrationModel without = ews_test::SpecModel();
    MinuteInput l2With = BaseMinute(&withNumbers);
    MinuteInput l2Without = BaseMinute(&without);
    l2With.packageGrowthUj = l2Without.packageGrowthUj = 120e6;
    l2With.dramGrowthUj = l2Without.dramGrowthUj = 30e6;
    MinuteInput l3With = BaseMinute(&withNumbers);
    MinuteInput l3Without = BaseMinute(&without);
    CHECK(SameMinute(CalculateMinuteEnergy(l2With), CalculateMinuteEnergy(l2Without)));
    CHECK(SameMinute(CalculateMinuteEnergy(l3With), CalculateMinuteEnergy(l3Without)));
    CHECK(!CalculateMinuteEnergy(l2With).terms.gpuW.has_value());
    CHECK(!CalculateMinuteEnergy(l3With).terms.gpuW.has_value());
}

EWS_TEST(EnergyBothGraphicsNumbersZeroIsBitForBitTheOldResult) {
    ews::CalibrationModel model = ews_test::SpecModel();
    MinuteInput l2Known = BaseMinute(&model);
    MinuteInput l2Unknown = BaseMinute(&model);
    l2Known.packageGrowthUj = l2Unknown.packageGrowthUj = 120e6;
    l2Known.dramGrowthUj = l2Unknown.dramGrowthUj = 30e6;
    l2Known.gpuPercent = 80.0;
    MinuteInput l3Known = BaseMinute(&model);
    MinuteInput l3Unknown = BaseMinute(&model);
    l3Known.gpuPercent = 100.0;
    CHECK(SameMinute(CalculateMinuteEnergy(l2Known), CalculateMinuteEnergy(l2Unknown)));
    CHECK(SameMinute(CalculateMinuteEnergy(l3Known), CalculateMinuteEnergy(l3Unknown)));
    // The term is known and 0 W, so the diagnostics line can show it.
    CHECK(CalculateMinuteEnergy(l2Known).terms.gpuW.has_value() && *CalculateMinuteEnergy(l2Known).terms.gpuW == 0.0);
}

EWS_TEST(EnergyL2UsesTheRestNumberAndL3UsesTheGpuNumber) {
    ews::CalibrationModel model = GraphicsModel();
    model.restWPerGpuPct = 0.0;
    MinuteInput l2 = BaseMinute(&model);
    l2.packageGrowthUj = 120e6;
    l2.dramGrowthUj = 30e6;
    l2.gpuPercent = 100.0;
    // L2 with rest_w_per_gpu_pct 0: the result is the one of EnergyL2WithDisplayOn.
    CHECK(Near(*CalculateMinuteEnergy(l2).deviceUj, 397920000.0));
    model.restWPerGpuPct = 0.03;
    model.gpuWPerPct = 0.0;
    MinuteInput l3 = BaseMinute(&model);
    l3.gpuPercent = 100.0;
    // L3 with gpu_w_per_pct 0: the result is the one of the L3 test, 401 520 000 uJ.
    CHECK(Near(*CalculateMinuteEnergy(l3).deviceUj, 401520000.0));
}

EWS_TEST(EnergyAGpuUseOutsideZeroToHundredCountsAsUnknown) {
    ews::CalibrationModel model = GraphicsModel();
    MinuteInput base = BaseMinute(&model);
    MinuteEnergy reference = CalculateMinuteEnergy(base);
    for (double bad : {-1.0, 100.5, 1e300, std::nan(""), std::numeric_limits<double>::infinity()}) {
        MinuteInput in = BaseMinute(&model);
        in.gpuPercent = bad;
        CHECK(SameMinute(CalculateMinuteEnergy(in), reference));
        CHECK(!CalculateMinuteEnergy(in).terms.gpuW.has_value());
    }
}

EWS_TEST(EnergyTheGpuUseDoesNotChangeL1) {
    MinuteInput in = BaseMinute(nullptr);
    in.systemGrowthUj = 300e6;
    in.gpuPercent = 90.0;
    MinuteEnergy e = CalculateMinuteEnergy(in);
    CHECK(e.level == MeasureLevel::L1);
    CHECK(e.deviceUj.has_value() && Near(*e.deviceUj, 300e6));
    CHECK(!e.terms.gpuW.has_value());
}
