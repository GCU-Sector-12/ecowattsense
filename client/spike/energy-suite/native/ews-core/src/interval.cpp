// interval.cpp: the report interval accumulator (see ews/interval.h and SPEC 4.1 to 4.5).
// Used by ews-service. Minutes are summed as floating values; the sum is rounded to whole uJ once, in Close.
#include "ews/interval.h"

#include <algorithm>
#include <cmath>

namespace ews {

namespace {

constexpr std::int64_t kMicrojoulesPerMwh = 3600000;

/// The largest energy sum that is still trusted. A bigger value is a sign of a bad counter, so it is unknown.
constexpr double kMaxEnergySumUj = 1.0e18;

/// @return True when the length is a usable length of a minute.
bool IsUsableSeconds(double seconds) { return std::isfinite(seconds) && seconds > 0.0; }

/// @return True when the value is a finite number that is not negative.
bool IsUsableEnergy(const std::optional<double>& value) { return value.has_value() && std::isfinite(*value) && *value >= 0.0; }

/// @return True when the battery change can be calculated from these two readings.
bool ChangeIsKnown(const BatteryReading& start, const BatteryReading& end) {
    if (!start.present || !end.present || !start.unitMwh || !end.unitMwh) {
        return false;
    }
    if (start.count != end.count || start.count <= 0) {
        return false;
    }
    return start.remainingMwh.has_value() && end.remainingMwh.has_value();
}

}  // namespace

IntervalAccumulator::IntervalAccumulator(std::int64_t startUnix, bool onBattery, const BatteryReading& startBattery)
    : startUnix_(startUnix), onBattery_(onBattery), startBattery_(startBattery) {}

void IntervalAccumulator::AddMinute(const IntervalMinute& minute) {
    if (!IsUsableSeconds(minute.seconds)) {
        return;
    }
    anyMinute_ = true;
    AddBatteryOf(minute);
    if (minute.cpuPercent.has_value() && std::isfinite(*minute.cpuPercent)) {
        cpuWeightedSum_ += *minute.cpuPercent * minute.seconds;
        cpuSeconds_ += minute.seconds;
    }
    if (!IsUsableEnergy(minute.deviceUj)) {
        unknownMinute_ = true;
        return;
    }
    deviceSumUj_ += *minute.deviceUj;
}

void IntervalAccumulator::AddBatteryOf(const IntervalMinute& minute) {
    if (minute.batteryJump) {
        batteryJumpSeen_ = true;
        heldJumpSeconds_ += minute.seconds;
        return;
    }
    batteryUnknownMinute_ = batteryUnknownMinute_ || !minute.batteryUj.has_value();
    batterySumUj_ += minute.batteryUj.value_or(0.0);
}

void IntervalAccumulator::ResolveBatteryJumps(double microjoulesPerSecond) {
    batterySumUj_ += microjoulesPerSecond * heldJumpSeconds_;
    heldJumpSeconds_ = 0;
}

std::optional<double> IntervalAccumulator::CpuPercent() const {
    if (cpuSeconds_ <= 0.0) {
        return std::nullopt;
    }
    return std::clamp(cpuWeightedSum_ / cpuSeconds_, 0.0, 100.0) + 0.0;
}

std::optional<std::int64_t> IntervalAccumulator::DeviceEnergyUj() const {
    if (!anyMinute_ || unknownMinute_) {
        return std::nullopt;
    }
    if (!std::isfinite(deviceSumUj_) || deviceSumUj_ > kMaxEnergySumUj) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(std::llround(deviceSumUj_));
}

std::optional<std::int64_t> IntervalAccumulator::BatteryChangeUj(const BatteryReading& endBattery) const {
    if (!ChangeIsKnown(startBattery_, endBattery)) {
        return std::nullopt;
    }
    if (batteryJumpSeen_) {
        return SumOfMinutesUj();
    }
    std::int64_t before = static_cast<std::int64_t>(*startBattery_.remainingMwh);
    std::int64_t after = static_cast<std::int64_t>(*endBattery.remainingMwh);
    return (after - before) * kMicrojoulesPerMwh;
}

std::optional<std::int64_t> IntervalAccumulator::SumOfMinutesUj() const {
    // A minute still held, or a minute without a believed change, leaves the whole interval unknown.
    bool known = heldJumpSeconds_ <= 0.0 && !batteryUnknownMinute_ && std::isfinite(batterySumUj_);
    if (!known) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(std::llround(batterySumUj_));
}

void IntervalAccumulator::FillOnBattery(IntervalResult& result, const std::optional<std::int64_t>& change) const {
    result.energyUj = 0;  // the server contract: energy_uj is 0 on battery
    bool usable = change.has_value() && *change <= 0;  // a charge that rose on battery is a bad reading
    if (!usable) {
        return;
    }
    result.batteryEnergyUj = *change;
    result.deviceEnergyUj = -*change;
}

void IntervalAccumulator::FillOnMains(IntervalResult& result, const std::optional<std::int64_t>& change,
                                      bool batteryPresent) const {
    std::optional<std::int64_t> device = DeviceEnergyUj();
    result.deviceEnergyUj = device;
    result.batteryEnergyUj = change;
    if (!device.has_value()) {
        return;
    }
    if (!batteryPresent) {
        result.energyUj = device;
        return;
    }
    if (!change.has_value()) {
        return;
    }
    result.energyUj = *device + std::max<std::int64_t>(0, *change);
}

IntervalResult IntervalAccumulator::Close(std::int64_t endUnix, const BatteryReading& endBattery) const {
    IntervalResult result;
    result.intervalSeconds = endUnix - startUnix_;
    if (result.intervalSeconds < 1) {
        result.dropped = true;
        return result;
    }
    result.endUnix = endUnix;
    result.onBattery = onBattery_;
    result.cpuPercent = CpuPercent();
    std::optional<std::int64_t> change = BatteryChangeUj(endBattery);
    if (onBattery_) {
        FillOnBattery(result, change);
        return result;
    }
    FillOnMains(result, change, startBattery_.present || endBattery.present);
    return result;
}

}  // namespace ews
