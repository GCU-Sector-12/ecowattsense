// test_interval.cpp: tests of the report interval accumulator (ews/interval.h, SPEC 4.1 to 4.5).
// 1 mWh = 3 600 000 uJ, so 20 mWh = 72 000 000 uJ.
#include "ews/interval.h"
#include "test_calc_common.h"
#include "test_common.h"

using ews::BatteryReading;
using ews::IntervalAccumulator;
using ews::IntervalMinute;
using ews::IntervalResult;

namespace {

constexpr std::int64_t kStart = 1000;
constexpr std::int64_t kEnd = 1300;

/// @return A battery in mWh with one cell and the given charge.
BatteryReading Battery(std::uint32_t mwh) {
    BatteryReading b;
    b.present = true;
    b.count = 1;
    b.unitMwh = true;
    b.remainingMwh = mwh;
    return b;
}

/// @return No battery at all.
BatteryReading NoBattery() { return BatteryReading{}; }

/// @return A minute of 60 seconds with the given energy.
IntervalMinute Minute(double uj) {
    IntervalMinute m;
    m.seconds = 60;
    m.cpuPercent = 10.0;
    m.deviceUj = uj;
    return m;
}

/// Adds five minutes of 100 000 000 uJ (sum 500 000 000 uJ).
void AddFiveMinutes(IntervalAccumulator& acc) {
    acc.AddMinute(Minute(100e6));
    acc.AddMinute(Minute(100e6));
    acc.AddMinute(Minute(100e6));
    acc.AddMinute(Minute(100e6));
    acc.AddMinute(Minute(100e6));
}

}  // namespace

EWS_TEST(IntervalOnBatteryDischarge) {
    IntervalAccumulator acc(kStart, true, Battery(30000));
    AddFiveMinutes(acc);
    IntervalResult r = acc.Close(kEnd, Battery(29980));
    CHECK(!r.dropped);
    CHECK(r.intervalSeconds == 300);
    CHECK(r.onBattery);
    CHECK(r.batteryEnergyUj == -72000000);
    CHECK(r.deviceEnergyUj == 72000000);
    CHECK(r.energyUj == 0);
}

EWS_TEST(IntervalOnBatteryChargeRisesLeavesBatteryFieldsOut) {
    IntervalAccumulator acc(kStart, true, Battery(30000));
    AddFiveMinutes(acc);
    IntervalResult r = acc.Close(kEnd, Battery(30005));
    CHECK(!r.batteryEnergyUj.has_value());
    CHECK(!r.deviceEnergyUj.has_value());
    CHECK(r.energyUj == 0);
}

EWS_TEST(IntervalOnBatteryRelativeUnit) {
    BatteryReading start = Battery(50);
    start.unitMwh = false;
    BatteryReading end = Battery(40);
    end.unitMwh = false;
    IntervalAccumulator acc(kStart, true, start);
    AddFiveMinutes(acc);
    IntervalResult r = acc.Close(kEnd, end);
    CHECK(!r.batteryEnergyUj.has_value());
    CHECK(!r.deviceEnergyUj.has_value());
    CHECK(r.energyUj == 0);
}

EWS_TEST(IntervalMainsCharging) {
    // +10 mWh = 36 000 000 uJ; energy = 500 000 000 + 36 000 000.
    IntervalAccumulator acc(kStart, false, Battery(30000));
    AddFiveMinutes(acc);
    IntervalResult r = acc.Close(kEnd, Battery(30010));
    CHECK(r.deviceEnergyUj == 500000000);
    CHECK(r.batteryEnergyUj == 36000000);
    CHECK(r.energyUj == 536000000);
}

EWS_TEST(IntervalMainsDischargeAddsNothing) {
    // -10 mWh on mains: only the positive part counts, so energy = device energy.
    IntervalAccumulator acc(kStart, false, Battery(30000));
    AddFiveMinutes(acc);
    IntervalResult r = acc.Close(kEnd, Battery(29990));
    CHECK(r.batteryEnergyUj == -36000000);
    CHECK(r.energyUj == 500000000);
}

EWS_TEST(IntervalMainsFullBattery) {
    IntervalAccumulator acc(kStart, false, Battery(36000));
    AddFiveMinutes(acc);
    IntervalResult r = acc.Close(kEnd, Battery(36000));
    CHECK(r.batteryEnergyUj == 0);
    CHECK(r.deviceEnergyUj == 500000000);
    CHECK(r.energyUj == 500000000);
}

EWS_TEST(IntervalMainsBatteryChangeUnknown) {
    BatteryReading end = Battery(0);
    end.remainingMwh.reset();
    IntervalAccumulator acc(kStart, false, Battery(30000));
    AddFiveMinutes(acc);
    IntervalResult r = acc.Close(kEnd, end);
    CHECK(r.deviceEnergyUj == 500000000);
    CHECK(!r.batteryEnergyUj.has_value());
    CHECK(!r.energyUj.has_value());
}

EWS_TEST(IntervalMainsWithoutBattery) {
    IntervalAccumulator acc(kStart, false, NoBattery());
    AddFiveMinutes(acc);
    IntervalResult r = acc.Close(kEnd, NoBattery());
    CHECK(r.deviceEnergyUj == 500000000);
    CHECK(!r.batteryEnergyUj.has_value());
    CHECK(r.energyUj == 500000000);
}

EWS_TEST(IntervalOneUnknownMinute) {
    IntervalAccumulator acc(kStart, false, Battery(30000));
    acc.AddMinute(Minute(100e6));
    IntervalMinute unknown = Minute(0);
    unknown.deviceUj.reset();
    acc.AddMinute(unknown);
    acc.AddMinute(Minute(100e6));
    IntervalResult r = acc.Close(kEnd, Battery(30010));
    CHECK(!r.deviceEnergyUj.has_value());
    CHECK(!r.energyUj.has_value());
    CHECK(r.batteryEnergyUj == 36000000);
}

EWS_TEST(IntervalWithoutAnyMinute) {
    IntervalAccumulator acc(kStart, false, NoBattery());
    IntervalResult r = acc.Close(kEnd, NoBattery());
    CHECK(!r.dropped);
    CHECK(!r.deviceEnergyUj.has_value());
    CHECK(!r.energyUj.has_value());
    CHECK(!r.cpuPercent.has_value());
}

EWS_TEST(IntervalBatteryCountChanged) {
    BatteryReading end = Battery(30010);
    end.count = 2;
    IntervalAccumulator acc(kStart, false, Battery(30000));
    AddFiveMinutes(acc);
    IntervalResult r = acc.Close(kEnd, end);
    CHECK(!r.batteryEnergyUj.has_value());
    CHECK(!r.energyUj.has_value());
    CHECK(r.deviceEnergyUj == 500000000);
}

EWS_TEST(IntervalShorterThanOneSecondIsDropped) {
    IntervalAccumulator acc(kStart, false, NoBattery());
    CHECK(acc.Close(kStart, NoBattery()).dropped);
    CHECK(acc.Close(kStart - 5, NoBattery()).dropped);
    CHECK(!acc.Close(kStart + 1, NoBattery()).dropped);
}

EWS_TEST(IntervalCpuPercentWeightedByMinuteLength) {
    // (10 x 60 + 40 x 30) / 90 = 20.0
    IntervalAccumulator acc(kStart, false, NoBattery());
    IntervalMinute a = Minute(1e6);
    a.seconds = 60;
    a.cpuPercent = 10.0;
    IntervalMinute b = Minute(1e6);
    b.seconds = 30;
    b.cpuPercent = 40.0;
    acc.AddMinute(a);
    acc.AddMinute(b);
    IntervalResult r = acc.Close(kEnd, NoBattery());
    CHECK(r.cpuPercent.has_value() && ews_test::Near(*r.cpuPercent * 1000.0, 20000.0));
}

EWS_TEST(IntervalRoundsOnlyAtTheEnd) {
    // Three minutes of 0.4 uJ: rounding each gives 0, rounding the sum 1.2 gives 1.
    IntervalAccumulator acc(kStart, false, NoBattery());
    acc.AddMinute(Minute(0.4));
    acc.AddMinute(Minute(0.4));
    acc.AddMinute(Minute(0.4));
    IntervalResult r = acc.Close(kEnd, NoBattery());
    CHECK(r.deviceEnergyUj == 1);
}

EWS_TEST(IntervalIgnoresMinuteOfZeroSeconds) {
    IntervalAccumulator acc(kStart, false, NoBattery());
    IntervalMinute zero = Minute(5e6);
    zero.seconds = 0;
    acc.AddMinute(zero);
    acc.AddMinute(Minute(100e6));
    IntervalResult r = acc.Close(kEnd, NoBattery());
    CHECK(r.deviceEnergyUj == 100000000);
}

EWS_TEST(IntervalNegativeMinuteEnergyIsUnknown) {
    IntervalAccumulator acc(kStart, false, NoBattery());
    acc.AddMinute(Minute(-1.0));
    IntervalResult r = acc.Close(kEnd, NoBattery());
    CHECK(!r.deviceEnergyUj.has_value());
}
