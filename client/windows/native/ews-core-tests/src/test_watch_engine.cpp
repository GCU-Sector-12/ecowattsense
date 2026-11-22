// test_watch_engine.cpp: tests of the watch state (SPEC 1.6): energy since the first good read, counters that go back,
// failed reads, NVML energy switched off, GPU use limits.
#include <cmath>
#include <optional>
#include <vector>

#include "ews/watch_engine.h"
#include "test_common.h"

using ews::WatchEngine;
using ews::WatchInput;

namespace {

using U64 = std::optional<std::uint64_t>;

/// Builds an input with one EMI channel.
WatchInput EmiOnly(std::int64_t tMs, U64 value) {
    WatchInput input;
    input.tMs = tMs;
    input.emiUj = {value};
    return input;
}

}  // namespace

EWS_TEST(WatchEngineFirstGoodReadIsZero) {
    WatchEngine engine(1, {}, 0);
    auto out = engine.Step(EmiOnly(0, 123456));
    CHECK(out.emiUj.size() == 1);
    CHECK(out.emiUj.size() == 1 && out.emiUj[0].value_or(99) == 0);
    auto next = engine.Step(EmiOnly(2000, 123556));
    CHECK(next.emiUj.size() == 1 && next.emiUj[0].value_or(99) == 100);
    CHECK(next.tMs == 2000);
}

EWS_TEST(WatchEngineFailedReadIsNullAndKeepsTheStart) {
    WatchEngine engine(1, {}, 0);
    engine.Step(EmiOnly(0, 1000));
    auto failed = engine.Step(EmiOnly(2000, std::nullopt));
    CHECK(failed.emiUj.size() == 1 && !failed.emiUj[0].has_value());
    auto after = engine.Step(EmiOnly(4000, 1500));
    CHECK(after.emiUj.size() == 1 && after.emiUj[0].value_or(99) == 500);
}

EWS_TEST(WatchEngineFirstReadMayFail) {
    WatchEngine engine(1, {}, 0);
    auto first = engine.Step(EmiOnly(0, std::nullopt));
    CHECK(first.emiUj.size() == 1 && !first.emiUj[0].has_value());
    auto second = engine.Step(EmiOnly(2000, 700));
    CHECK(second.emiUj.size() == 1 && second.emiUj[0].value_or(99) == 0);
}

EWS_TEST(WatchEngineCounterGoingBackGivesNullOnceAndANewStart) {
    WatchEngine engine(1, {}, 0);
    engine.Step(EmiOnly(0, 1000));
    engine.Step(EmiOnly(2000, 1400));
    auto back = engine.Step(EmiOnly(4000, 50));
    CHECK(back.emiUj.size() == 1 && !back.emiUj[0].has_value());
    auto next = engine.Step(EmiOnly(6000, 80));
    CHECK(next.emiUj.size() == 1 && next.emiUj[0].value_or(99) == 30);
}

EWS_TEST(WatchEngineChannelsAreIndependent) {
    WatchEngine engine(2, {}, 0);
    WatchInput a;
    a.emiUj = {10, 500};
    engine.Step(a);
    WatchInput b;
    b.emiUj = {25, std::nullopt};
    auto out = engine.Step(b);
    CHECK(out.emiUj.size() == 2);
    CHECK(out.emiUj.size() == 2 && out.emiUj[0].value_or(99) == 15);
    CHECK(out.emiUj.size() == 2 && !out.emiUj[1].has_value());
}

EWS_TEST(WatchEngineShortInputListsCountAsNull) {
    WatchEngine engine(2, {true}, 2);
    auto out = engine.Step(WatchInput{});
    CHECK(out.emiUj.size() == 2 && !out.emiUj[0].has_value() && !out.emiUj[1].has_value());
    CHECK(out.nvmlMw.size() == 1 && !out.nvmlMw[0].has_value());
    CHECK(out.nvmlMj.size() == 1 && !out.nvmlMj[0].has_value());
    CHECK(out.gpuPct.size() == 2 && !out.gpuPct[0].has_value() && !out.gpuPct[1].has_value());
}

EWS_TEST(WatchEngineNvmlPowerPassesThroughAndEnergyIsSinceStart) {
    WatchEngine engine(0, {true}, 0);
    WatchInput first;
    first.nvmlMw = {15300};
    first.nvmlMj = {98765};
    auto one = engine.Step(first);
    CHECK(one.nvmlMw.size() == 1 && one.nvmlMw[0].value_or(0) == 15300);
    CHECK(one.nvmlMj.size() == 1 && one.nvmlMj[0].value_or(99) == 0);
    WatchInput second;
    second.nvmlMw = {std::nullopt};
    second.nvmlMj = {99000};
    auto two = engine.Step(second);
    CHECK(two.nvmlMw.size() == 1 && !two.nvmlMw[0].has_value());
    CHECK(two.nvmlMj.size() == 1 && two.nvmlMj[0].value_or(99) == 235);
}

EWS_TEST(WatchEngineNvmlEnergyIsAlwaysNullWhenTheHeaderSaysNo) {
    WatchEngine engine(0, {false}, 0);
    WatchInput input;
    input.nvmlMj = {98765};
    CHECK(engine.Step(input).nvmlMj.size() == 1);
    CHECK(!engine.Step(input).nvmlMj[0].has_value());
}

EWS_TEST(WatchEngineGpuUseIsLimitedAndMustBeFinite) {
    WatchEngine engine(0, {}, 4);
    WatchInput input;
    input.gpuPct = {12.5, 250.0, -3.0, std::nullopt};
    auto out = engine.Step(input);
    CHECK(out.gpuPct.size() == 4);
    CHECK(out.gpuPct.size() == 4 && out.gpuPct[0].value_or(-1) == 12.5);
    CHECK(out.gpuPct.size() == 4 && out.gpuPct[1].value_or(-1) == 100.0);
    CHECK(out.gpuPct.size() == 4 && out.gpuPct[2].value_or(-1) == 0.0);
    CHECK(out.gpuPct.size() == 4 && !out.gpuPct[3].has_value());
    WatchInput nan;
    nan.gpuPct = {std::nan("")};
    WatchEngine other(0, {}, 1);
    CHECK(!other.Step(nan).gpuPct[0].has_value());
}
