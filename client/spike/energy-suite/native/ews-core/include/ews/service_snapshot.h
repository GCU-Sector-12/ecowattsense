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

/// @param time A time.
/// @return Unix seconds (whole seconds, rounded down).
std::int64_t ToUnixSeconds(UtcTime time);

}  // namespace ews
