// service_snapshot.cpp: the pure rules over two snapshots (see ews/service_snapshot.h).
// Used by ServiceCore. EMI counters are in pWh; the sums are converted to uJ once, at the end.
#include "ews/service_snapshot.h"

#include <chrono>
#include <cstddef>

namespace ews {

namespace {

/// The running sum of one kind over its channels.
struct KindSum {
    int channels = 0;
    bool bad = false;
    std::uint64_t picowattHours = 0;
};

/// Adds the growth of one channel to the sum when it is of the wanted kind.
/// A sample that is not known, or a counter that went backwards, marks the whole kind bad.
void AddChannel(KindSum& sum, const EmiChannel& channel, EmiKind kind, const EmiSample& before, const EmiSample& after) {
    if (channel.kind != kind) {
        return;
    }
    ++sum.channels;
    bool usable = before.known && after.known && after.energyPicowattHours >= before.energyPicowattHours;
    sum.bad = sum.bad || !usable;
    sum.picowattHours += usable ? after.energyPicowattHours - before.energyPicowattHours : 0;
}

/// @return The growth of one kind in uJ, or nothing when it is unknown.
std::optional<double> KindGrowth(const std::vector<EmiChannel>& channels, const Snapshot& previous, const Snapshot& current,
                                 EmiKind kind) {
    bool sizesMatch = previous.emi.size() == channels.size() && current.emi.size() == channels.size();
    if (!previous.emiKnown || !current.emiKnown || !sizesMatch) {
        return std::nullopt;
    }
    KindSum sum;
    for (std::size_t index = 0; index < channels.size(); ++index) {
        AddChannel(sum, channels[index], kind, previous.emi[index], current.emi[index]);
    }
    if (sum.channels == 0 || sum.bad) {
        return std::nullopt;
    }
    Reading<std::uint64_t> micro = PicowattHoursToMicrojoules(sum.picowattHours);
    return micro.ok() ? std::optional<double>(static_cast<double>(micro.value)) : std::nullopt;
}

/// Adds one battery to the summary.
void AddBattery(BatteryReading& summary, bool& allKnown, const BatteryState& battery) {
    if (battery.unit != BatteryUnit::MilliwattHours) {
        return;
    }
    summary.unitMwh = true;
    allKnown = allKnown && battery.remainingKnown;
    summary.remainingMwh = summary.remainingMwh.value_or(0) + (battery.remainingKnown ? battery.remaining : 0u);
}

}  // namespace

EmiGrowth ComputeEmiGrowth(const std::vector<EmiChannel>& channels, const Snapshot& previous, const Snapshot& current) {
    EmiGrowth growth;
    growth.systemUj = KindGrowth(channels, previous, current, EmiKind::System);
    growth.packageUj = KindGrowth(channels, previous, current, EmiKind::CpuPackage);
    growth.dramUj = KindGrowth(channels, previous, current, EmiKind::Dram);
    return growth;
}

BatteryReading ToBatteryReading(const Reading<std::vector<BatteryState>>& read) {
    BatteryReading summary;
    if (!read.ok()) {
        summary.present = true;
        return summary;
    }
    summary.present = !read.value.empty();
    summary.count = static_cast<int>(read.value.size());
    bool allKnown = true;
    for (const BatteryState& battery : read.value) {
        AddBattery(summary, allKnown, battery);
    }
    if (!allKnown) {
        summary.remainingMwh.reset();
    }
    return summary;
}

std::optional<double> BatteryChangeUj(const BatteryReading& previous, const BatteryReading& current) {
    bool known = previous.present && current.present && previous.unitMwh && current.unitMwh;
    known = known && previous.count == current.count && previous.count > 0;
    known = known && previous.remainingMwh.has_value() && current.remainingMwh.has_value();
    if (!known) {
        return std::nullopt;
    }
    double change = static_cast<double>(*current.remainingMwh) - static_cast<double>(*previous.remainingMwh);
    return change * 3600000.0;
}

std::int64_t ToUnixSeconds(UtcTime time) {
    return std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
}

}  // namespace ews
