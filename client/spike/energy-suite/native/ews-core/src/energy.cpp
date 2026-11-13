// energy.cpp: the energy of one minute (see ews/energy.h and SPEC 4.2 and 4.5).
// Used by ews-service at every tick. Every function is pure: values in, values out.
#include "ews/energy.h"

#include <algorithm>
#include <cmath>

namespace ews {

namespace {

constexpr double kMicrojoulesPerJoule = 1000000.0;
constexpr double kDefaultBrightnessPct = 50.0;

/// @return True when the value is known, finite and not negative. A negative growth means the counter went backwards.
bool IsGrowth(const std::optional<double>& value) { return value.has_value() && std::isfinite(*value) && *value >= 0.0; }

/// @return True when the value is known and between 0 and 100.
bool IsPercent(const std::optional<double>& value) {
    return value.has_value() && std::isfinite(*value) && *value >= 0.0 && *value <= 100.0;
}

/// @return The value when `wanted` and known, else 0.
double GrowthIfWanted(bool wanted, const std::optional<double>& value) { return wanted ? value.value_or(0.0) : 0.0; }

/// @return A minute with no value.
MinuteEnergy UnknownMinute() { return MinuteEnergy{}; }

/// @return A calculated minute.
MinuteEnergy KnownMinute(MeasureLevel level, double deviceUj, double measuredUj) {
    MinuteEnergy minute;
    minute.level = level;
    minute.deviceUj = deviceUj;
    minute.measuredUj = measuredUj;
    return minute;
}

/// @return The brightness to use: the value when it is between 0 and 100, else 50.
double BrightnessOrDefault(const std::optional<double>& brightness) {
    if (!IsPercent(brightness)) {
        return kDefaultBrightnessPct;
    }
    return *brightness;
}

/// @return Screen power in W: 0 with the display off, else screen_w_at_50 + (brightness - 50) x screen_w_per_pct, not below 0.
double ScreenWatts(const MinuteInput& input, const CalibrationModel& model) {
    if (!input.displayOn) {
        return 0.0;
    }
    double brightness = BrightnessOrDefault(input.brightnessPct);
    return std::max(0.0, model.screenWAt50 + (brightness - kDefaultBrightnessPct) * model.screenWPerPct);
}

/// @return True when the model names a measured part and the growth of every named part is known.
bool MeasuredPartsKnown(const MinuteInput& input, const CalibrationModel& model) {
    if (!model.measuresCpuPackage && !model.measuresDram) {
        return false;
    }
    if (model.measuresCpuPackage && !IsGrowth(input.packageGrowthUj)) {
        return false;
    }
    return !model.measuresDram || IsGrowth(input.dramGrowthUj);
}

/// Level L2: measured parts from EMI, the rest from the model. Unknown when a needed value is missing.
MinuteEnergy TryLevel2(const MinuteInput& input, const CalibrationModel& model) {
    if (!MeasuredPartsKnown(input, model) || !IsPercent(input.cpuPercent)) {
        return UnknownMinute();
    }
    double measured = GrowthIfWanted(model.measuresCpuPackage, input.packageGrowthUj) +
                      GrowthIfWanted(model.measuresDram, input.dramGrowthUj);
    double restWatts = model.restW + model.restWPerCpuPct * *input.cpuPercent + ScreenWatts(input, model);
    return KnownMinute(MeasureLevel::L2, measured + restWatts * input.seconds * kMicrojoulesPerJoule, measured);
}

/// Level L3: everything from the model. Unknown when the processor percent is unknown.
MinuteEnergy TryLevel3(const MinuteInput& input, const CalibrationModel& model) {
    if (!IsPercent(input.cpuPercent)) {
        return UnknownMinute();
    }
    double watts = model.baseW + model.cpuWPerPct * *input.cpuPercent + ScreenWatts(input, model);
    return KnownMinute(MeasureLevel::L3, watts * input.seconds * kMicrojoulesPerJoule, 0.0);
}

}  // namespace

const char* MeasureLevelWord(MeasureLevel level) {
    switch (level) {
        case MeasureLevel::L1: return "L1";
        case MeasureLevel::L2: return "L2";
        case MeasureLevel::L3: return "L3";
        default: return "";
    }
}

MinuteEnergy CalculateMinuteEnergy(const MinuteInput& input) {
    if (!std::isfinite(input.seconds) || input.seconds <= 0.0) {
        return UnknownMinute();
    }
    if (IsGrowth(input.systemGrowthUj)) {
        return KnownMinute(MeasureLevel::L1, *input.systemGrowthUj, *input.systemGrowthUj);
    }
    if (input.model == nullptr) {
        return UnknownMinute();
    }
    MinuteEnergy level2 = TryLevel2(input, *input.model);
    if (level2.level != MeasureLevel::Unknown) {
        return level2;
    }
    return TryLevel3(input, *input.model);
}

}  // namespace ews
