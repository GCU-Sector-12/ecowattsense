// service_snapshot.h: one read of every cumulative counter, and the pure rules that turn two reads into a minute.
// Used by ServiceCore (service_core.h) at every tick, and by the tests. No Windows call is made here.
// An unknown value stays unknown: a failed read, a counter that went backwards or a missing channel never becomes 0.
#pragma once

#include <optional>
#include <vector>

#include "ews/battery.h"
#include "ews/cpu_times.h"
#include "ews/emi.h"
#include "ews/interval.h"
#include "ews/reading.h"
#include "ews/utc_stamp.h"

namespace ews {

/// Every cumulative counter at one moment.
struct Snapshot {
    UtcTime time{};                    ///< When the counters were read.
    bool emiKnown = false;             ///< The EMI read worked and gave one sample per channel.
    std::vector<EmiSample> emi;        ///< One sample per channel, in the order of the channel list.
    BatteryReading battery;            ///< The battery summary.
    std::optional<CpuTimes> cpu;       ///< The processor times, or nothing when the read failed.
};

/// Growth of the EMI kinds between two snapshots, in uJ. An empty value is unknown.
struct EmiGrowth {
    std::optional<double> systemUj;
    std::optional<double> packageUj;
    std::optional<double> dramUj;
};

/// Sums the growth of every channel of each counted kind (system, cpu_package, dram).
/// A kind is unknown when it has no channel, a sample of it is unknown, or one of its counters went backwards.
/// @param channels The channel list from IEmiSource::Open.
/// @param previous The earlier snapshot.
/// @param current The later snapshot.
/// @return The growth of the three kinds.
EmiGrowth ComputeEmiGrowth(const std::vector<EmiChannel>& channels, const Snapshot& previous, const Snapshot& current);

/// Sums the batteries into the one reading the interval accumulator needs. The unit is mWh when any battery reports
/// mWh; the charge is the sum of the mWh batteries and is unknown when one of them does not know it.
/// A failed read gives `present` true with no charge, because batteries exist but could not be read.
/// @param read The result of IBatterySource::Read.
/// @return The summary.
BatteryReading ToBatteryReading(const Reading<std::vector<BatteryState>>& read);

/// The battery charge change between two snapshots in uJ (positive while charging).
/// @param previous The earlier battery reading.
/// @param current The later battery reading.
/// @return The change, or nothing when it is not known (unit, count or charge missing or changed).
std::optional<double> BatteryChangeUj(const BatteryReading& previous, const BatteryReading& current);

/// The largest battery power that is believed, in watts, in either direction. A laptop battery cannot charge or
/// discharge faster than this. Why: on the HP 250 G7 the gauge jumped up by about 670 mWh in 6 seconds when the
/// charger was unplugged (measured 14 Nov 2026), which looked like +419 W. A real charger or load stays far below.
inline constexpr double kMaxBatteryWatts = 200.0;

/// The battery change of one minute after the plausibility check of SPEC 4.3.
struct MinuteBattery {
    std::optional<double> changeUj;  ///< The change in uJ; empty when unknown or impossible.
    bool jump = false;               ///< True when the change was known but impossible (it is then not believed).
};

/// Judges the battery change between two snapshots. A known change is impossible when the charge rose while the
/// computer runs on battery, or when its power over the period is above kMaxBatteryWatts in either direction.
/// @param previous The earlier battery reading.
/// @param current The later battery reading.
/// @param onBattery True when the computer runs on battery during the period.
/// @param seconds The length of the period; a length of 0 or less makes any known change impossible.
/// @return The believed change (empty when unknown or impossible) and whether it was a jump.
MinuteBattery JudgeBatteryChange(const BatteryReading& previous, const BatteryReading& current, bool onBattery, double seconds);

/// @param time A time.
/// @return Unix seconds (whole seconds, rounded down).
std::int64_t ToUnixSeconds(UtcTime time);

}  // namespace ews
