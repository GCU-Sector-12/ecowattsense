// interval.h: the accumulator of one report interval (SPEC 4.1 to 4.4 and 4.5).
// Used by ews-service: it starts one at the start of an interval, adds every minute, and closes it when the report
// is due. The result holds the fields of the report that depend on energy. Rounding to whole uJ happens once, in Close.
#pragma once

#include <cstdint>
#include <optional>

namespace ews {

/// The battery as read at the start or at the end of an interval.
struct BatteryReading {
    bool present = false;                          ///< At least one battery exists.
    int count = 0;                                 ///< How many batteries.
    bool unitMwh = false;                          ///< The unit is mWh (not a relative number).
    std::optional<std::uint32_t> remainingMwh;     ///< Summed charge in mWh; empty when unknown.
};

/// One minute as the accumulator needs it.
struct IntervalMinute {
    double seconds = 0;                  ///< Length of the minute. A length of 0 or less is ignored.
    std::optional<double> cpuPercent;    ///< Processor load of the minute.
    std::optional<double> deviceUj;      ///< Energy of the minute; empty marks the whole interval's energy unknown.
};

/// The result of closing an interval. An empty optional is an unknown value that the report leaves out.
struct IntervalResult {
    bool dropped = false;                         ///< The interval is shorter than 1 second; send nothing.
    std::int64_t endUnix = 0;                     ///< End of the interval, Unix seconds.
    std::int64_t intervalSeconds = 0;             ///< Real length, whole seconds.
    bool onBattery = false;                       ///< The value of the whole interval.
    std::optional<double> cpuPercent;             ///< Average weighted by minute length, 0 to 100.
    std::optional<std::int64_t> energyUj;
    std::optional<std::int64_t> batteryEnergyUj;  ///< Charge change; negative when discharging.
    std::optional<std::int64_t> deviceEnergyUj;
};

/// Collects the minutes of one interval.
class IntervalAccumulator {
public:
    /// Starts an interval.
    /// @param startUnix Start time, Unix seconds.
    /// @param onBattery True when the computer runs on battery (the whole interval has this value).
    /// @param startBattery The battery at the start.
    IntervalAccumulator(std::int64_t startUnix, bool onBattery, const BatteryReading& startBattery);

    /// Adds one minute. A minute of 0 seconds or less, or with a non-finite length, is ignored.
    /// @param minute The minute.
    void AddMinute(const IntervalMinute& minute);

    /// Closes the interval.
    /// @param endUnix End time, Unix seconds.
    /// @param endBattery The battery at the end.
    /// @return The result. dropped is true when the interval is shorter than 1 second.
    IntervalResult Close(std::int64_t endUnix, const BatteryReading& endBattery) const;

private:
    std::optional<std::int64_t> BatteryChangeUj(const BatteryReading& endBattery) const;
    std::optional<std::int64_t> DeviceEnergyUj() const;
    std::optional<double> CpuPercent() const;
    void FillOnBattery(IntervalResult& result, const std::optional<std::int64_t>& change) const;
    void FillOnMains(IntervalResult& result, const std::optional<std::int64_t>& change, bool batteryPresent) const;

    std::int64_t startUnix_;
    bool onBattery_;
    BatteryReading startBattery_;
    double cpuWeightedSum_ = 0;   ///< Sum of percent x seconds over minutes with a known percent.
    double cpuSeconds_ = 0;       ///< Sum of seconds over those minutes.
    double deviceSumUj_ = 0;
    bool anyMinute_ = false;
    bool unknownMinute_ = false;
};

}  // namespace ews
