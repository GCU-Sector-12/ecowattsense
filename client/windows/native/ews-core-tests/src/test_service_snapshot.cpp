// test_service_snapshot.cpp: tests of the pure rules over two snapshots (EMI growth, battery summary and change).
#include <vector>

#include "ews/service_snapshot.h"
#include "fake_sources.h"
#include "test_common.h"

namespace {

/// Builds a snapshot with known EMI samples of the given counters.
ews::Snapshot WithCounters(const std::vector<std::uint64_t>& counters) {
    ews::Snapshot snapshot;
    snapshot.emiKnown = true;
    for (std::uint64_t value : counters) {
        snapshot.emi.push_back(ews_test::MakeSample(value, value));
    }
    return snapshot;
}

/// @return Two channels: package and memory.
std::vector<ews::EmiChannel> TwoChannels() {
    return {ews_test::MakeChannel(0, 0, "RAPL_Package0_PKG"), ews_test::MakeChannel(0, 1, "RAPL_Package0_DRAM")};
}

}  // namespace

EWS_TEST(SnapshotEmiGrowthByKind) {
    auto growth = ews::ComputeEmiGrowth(TwoChannels(), WithCounters({1000, 2000}), WithCounters({1000 + 1000000000ull, 2000 + 500000000ull}));
    CHECK(growth.packageUj.has_value() && *growth.packageUj == 3600000.0);
    CHECK(growth.dramUj.has_value() && *growth.dramUj == 1800000.0);
    CHECK(!growth.systemUj.has_value());
}

EWS_TEST(SnapshotEmiBackwardsCounterIsUnknownForItsKindOnly) {
    auto growth = ews::ComputeEmiGrowth(TwoChannels(), WithCounters({5000, 2000}), WithCounters({1000, 3000000000ull}));
    CHECK(!growth.packageUj.has_value());
    CHECK(growth.dramUj.has_value());
}

EWS_TEST(SnapshotEmiUnknownReadGivesUnknownGrowth) {
    ews::Snapshot unknown;
    auto growth = ews::ComputeEmiGrowth(TwoChannels(), WithCounters({1, 2}), unknown);
    CHECK(!growth.packageUj.has_value());
    CHECK(!growth.dramUj.has_value());
    ews::Snapshot wrongSize = WithCounters({1});
    growth = ews::ComputeEmiGrowth(TwoChannels(), WithCounters({1, 2}), wrongSize);
    CHECK(!growth.packageUj.has_value());
}

EWS_TEST(SnapshotBatterySummary) {
    ews::BatteryState one = ews_test::MakeMwhBattery(true);
    one.remainingKnown = true;
    one.remaining = 20000;
    ews::BatteryState two = one;
    two.remaining = 5000;
    auto summary = ews::ToBatteryReading(ews::Reading<std::vector<ews::BatteryState>>::Success({one, two}));
    CHECK(summary.present && summary.unitMwh && summary.count == 2);
    CHECK(summary.remainingMwh.has_value() && *summary.remainingMwh == 25000);
    two.remainingKnown = false;
    summary = ews::ToBatteryReading(ews::Reading<std::vector<ews::BatteryState>>::Success({one, two}));
    CHECK(!summary.remainingMwh.has_value());
}

EWS_TEST(SnapshotBatteryNoneAndFailed) {
    auto none = ews::ToBatteryReading(ews::Reading<std::vector<ews::BatteryState>>::Success({}));
    CHECK(!none.present);
    auto failed = ews::ToBatteryReading(ews::Reading<std::vector<ews::BatteryState>>::Failure(ews::Status::Error, 5));
    CHECK(failed.present && !failed.remainingMwh.has_value());
    ews::BatteryState relative;
    relative.unit = ews::BatteryUnit::Relative;
    relative.remainingKnown = true;
    relative.remaining = 50;
    auto summary = ews::ToBatteryReading(ews::Reading<std::vector<ews::BatteryState>>::Success({relative}));
    CHECK(summary.present && !summary.unitMwh && !summary.remainingMwh.has_value());
}

EWS_TEST(SnapshotBatteryChange) {
    ews::BatteryReading before;
    before.present = true;
    before.count = 1;
    before.unitMwh = true;
    before.remainingMwh = 1000;
    ews::BatteryReading after = before;
    after.remainingMwh = 990;
    auto change = ews::BatteryChangeUj(before, after);
    CHECK(change.has_value() && *change == -36000000.0);
    after.count = 2;
    CHECK(!ews::BatteryChangeUj(before, after).has_value());
    after.count = 1;
    after.remainingMwh.reset();
    CHECK(!ews::BatteryChangeUj(before, after).has_value());
}

namespace {

/// @return A one-cell mWh battery reading with the given charge.
ews::BatteryReading Charge(std::uint32_t mwh) {
    ews::BatteryReading reading;
    reading.present = true;
    reading.count = 1;
    reading.unitMwh = true;
    reading.remainingMwh = mwh;
    return reading;
}

}  // namespace

EWS_TEST(SnapshotJumpTheLiveBugOnBatteryIsUnknown) {
    // 35819 to 36491 mWh in 6 seconds on battery, measured on the HP 250 G7 on 14 Nov 2026.
    ews::MinuteBattery result = ews::JudgeBatteryChange(Charge(35819), Charge(36491), true, 6.0);
    CHECK(!result.changeUj.has_value());
    CHECK(result.jump);
}

EWS_TEST(SnapshotNormalDischargeOnBatteryIsKept) {
    ews::MinuteBattery result = ews::JudgeBatteryChange(Charge(1000), Charge(989), true, 60.0);
    CHECK(result.changeUj.has_value() && *result.changeUj == -39600000.0);
    CHECK(!result.jump);
}

EWS_TEST(SnapshotFastRiseOnMainsIsUnknownButNormalChargingIsKept) {
    ews::MinuteBattery fast = ews::JudgeBatteryChange(Charge(1000), Charge(1672), false, 6.0);
    CHECK(!fast.changeUj.has_value() && fast.jump);
    ews::MinuteBattery normal = ews::JudgeBatteryChange(Charge(1000), Charge(1300), false, 60.0);
    CHECK(normal.changeUj.has_value() && *normal.changeUj == 1080000000.0);
    CHECK(!normal.jump);
}

EWS_TEST(SnapshotFastDropOnBatteryIsUnknown) {
    ews::MinuteBattery result = ews::JudgeBatteryChange(Charge(5000), Charge(1000), true, 60.0);
    CHECK(!result.changeUj.has_value());
    CHECK(result.jump);
}

EWS_TEST(SnapshotRiseOnBatteryIsAJumpEvenWhenSlow) {
    ews::MinuteBattery result = ews::JudgeBatteryChange(Charge(1000), Charge(1001), true, 60.0);
    CHECK(!result.changeUj.has_value());
    CHECK(result.jump);
}

EWS_TEST(SnapshotUnknownChangeIsNotAJump) {
    ews::BatteryReading unknown = Charge(1000);
    unknown.remainingMwh.reset();
    ews::MinuteBattery result = ews::JudgeBatteryChange(Charge(1000), unknown, true, 60.0);
    CHECK(!result.changeUj.has_value());
    CHECK(!result.jump);
}
