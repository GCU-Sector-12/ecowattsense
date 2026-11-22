// test_service_gpu.cpp: tests of the GPU use in the service (SPEC 4.2, 5.3, 7a and 8, piece N2): the rule that turns one
// read of the counter set "GPU Engine" into the use of a minute, and ServiceCore with a scripted GPU source.
// The rig (service_rig.h) runs at level L2 with 5 W package and 0.5 W memory, 20 percent processor load and an unknown
// brightness (counted as 50). One minute with the model of schema 2 is 559 200 000 uJ. With the model of schema 3
// (rest_w_per_gpu_pct 0.03, gpu_w_per_pct 0.04) and a GPU use of 50 percent it is 559 200 000 + 0.03 x 50 W x 60 s
// = 649 200 000 uJ (10.82 W). The source script: the read of the start comes first, then one read for each tick.
#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>
#include <vector>

#include "service_rig.h"
#include "test_common.h"

using ews_test::Has;
using ews_test::Rig;
using ews_test::ScriptedGpuUse;

namespace {

/// Starts the rig and checks that it started.
void StartOk(Rig& rig) { CHECK(rig.core.Start() == ews::StartResult::Ok); }

/// Gives the rig the model of schema 3 (it replaces the schema 2 file the rig wrote). Call it before Start.
void UseGraphicsModel(Rig& rig) { ews_test::WriteText(rig.data.path() / "calibration" / "model.json", ews_test::SpecModelText3()); }

/// Tells the service that the scan found the source readable. Call it before Start.
void ScanSaysReadable(Rig& rig) { rig.scan.result.gpuUse.common.readable = ews::Tri::True; }

/// @return True when the line is a tick line.
bool IsTickLine(const std::string& line) { return line.compare(0, 12, "{\"t\":\"tick\",") == 0; }

/// @param rig The rig. @param index 0 for the first tick line. @return That tick line, or an empty text.
std::string TickLine(const Rig& rig, std::size_t index) {
    std::vector<std::string> ticks;
    std::copy_if(rig.diag.lines.begin(), rig.diag.lines.end(), std::back_inserter(ticks), IsTickLine);
    return index < ticks.size() ? ticks[index] : std::string();
}

/// @param pid The process id of the instance. @param engine The text after "eng_" (for example "0_engtype_3D").
/// @param percent The value. @return One instance of adapter 1.
ews::PdhInstance InstanceOf(int pid, const std::string& engine, double percent) {
    return ews::PdhInstance{"pid_" + std::to_string(pid) + "_luid_0x00000000_0x00000001_phys_0_eng_" + engine, percent};
}

}  // namespace

EWS_TEST(GpuUsePercentOfAFailedOrEmptyReadIsUnknown) {
    CHECK(!ews::GpuUsePercentOf(ScriptedGpuUse::Unavailable()).has_value());
    CHECK(!ews::GpuUsePercentOf(ScriptedGpuUse::Failed()).has_value());
    CHECK(!ews::GpuUsePercentOf(ScriptedGpuUse::Good({})).has_value());
}

EWS_TEST(GpuUsePercentOfAnAdapterWithoutAThreeDInstanceIsUnknown) {
    auto copyOnly = ScriptedGpuUse::Good({InstanceOf(1, "0_engtype_Copy", 30.0)});
    CHECK(!ews::GpuUsePercentOf(copyOnly).has_value());
}

EWS_TEST(GpuUsePercentOfTheHighestAdapterIsUsed) {
    auto two = ScriptedGpuUse::Good({ScriptedGpuUse::Engine(1, 30.0), ScriptedGpuUse::Engine(2, 70.0)});
    auto percent = ews::GpuUsePercentOf(two);
    CHECK(percent.has_value() && *percent == 70.0);
}

EWS_TEST(GpuUsePercentOfSumsProcessesAndTakesTheHighestEngine) {
    // Engine 0: 40 + 30 over two processes = 70. Engine 1: 20. The adapter is at 70.
    auto read = ScriptedGpuUse::Good({InstanceOf(10, "0_engtype_3D", 40.0), InstanceOf(11, "0_engtype_3D", 30.0), InstanceOf(10, "1_engtype_3D", 20.0)});
    auto percent = ews::GpuUsePercentOf(read);
    CHECK(percent.has_value() && *percent == 70.0);
}

EWS_TEST(GpuUsePercentOfIsLimitedToHundredAndIgnoresBadValues) {
    auto over = ews::GpuUsePercentOf(ScriptedGpuUse::Good({InstanceOf(10, "0_engtype_3D", 80.0), InstanceOf(11, "0_engtype_3D", 70.0)}));
    CHECK(over.has_value() && *over == 100.0);
    auto bad = ews::GpuUsePercentOf(ScriptedGpuUse::Good({InstanceOf(10, "0_engtype_3D", -5.0)}));
    CHECK(!bad.has_value());
    auto notANumber = ews::GpuUsePercentOf(ScriptedGpuUse::Good({InstanceOf(10, "0_engtype_3D", std::nan(""))}));
    CHECK(!notANumber.has_value());
}

EWS_TEST(ServiceGpuFirstReadIsUnknownThenTheValueIsUsed) {
    Rig rig("gpu-first");
    UseGraphicsModel(rig);
    ScanSaysReadable(rig);
    rig.gpuUse.script = {ScriptedGpuUse::Unavailable(), ScriptedGpuUse::Use(50.0)};
    StartOk(rig);
    CHECK(rig.gpuUse.reads == 1);  // The read of the start only starts the counter.
    rig.Ticks(1);
    CHECK(rig.gpuUse.reads == 2);
    std::string line = TickLine(rig, 0);
    CHECK(Has(line, "\"gpu\":{\"pct\":50.0,\"from\":\"service\"}"));
    CHECK(Has(line, "\"cpu_term_w\":0.180,\"gpu_term_w\":1.500,\"screen_w\":1.400,\"device_w\":10.820,\"energy_uj\":649200000"));
}

EWS_TEST(ServiceGpuAnUnknownFirstTickAddsNothing) {
    Rig rig("gpu-unknown");
    UseGraphicsModel(rig);
    ScanSaysReadable(rig);
    rig.gpuUse.script = {ScriptedGpuUse::Unavailable(), ScriptedGpuUse::Unavailable()};
    StartOk(rig);
    rig.Ticks(1);
    std::string line = TickLine(rig, 0);
    CHECK(Has(line, "\"gpu\":{\"from\":\"default\"}"));
    CHECK(!Has(line, "gpu_term_w"));
    CHECK(Has(line, "\"device_w\":9.320,\"energy_uj\":559200000"));
}

EWS_TEST(ServiceGpuSourceThatTheScanCannotReadIsNeverRead) {
    Rig rig("gpu-not-readable");
    UseGraphicsModel(rig);
    rig.gpuUse.script = {ScriptedGpuUse::Use(50.0), ScriptedGpuUse::Use(50.0), ScriptedGpuUse::Use(50.0)};
    StartOk(rig);
    rig.Ticks(2);
    CHECK(rig.gpuUse.reads == 0);
    std::string line = TickLine(rig, 1);
    CHECK(Has(line, "\"gpu\":{\"from\":\"default\"}"));
    CHECK(!Has(line, "gpu_term_w"));
    CHECK(Has(line, "\"energy_uj\":559200000"));
    Rig denied("gpu-denied");
    denied.scan.result.gpuUse.common.readable = ews::Tri::False;
    denied.gpuUse.script = {ScriptedGpuUse::Use(50.0)};
    StartOk(denied);
    denied.Ticks(1);
    CHECK(denied.gpuUse.reads == 0);
}

EWS_TEST(ServiceGpuWithoutASourceNeverStopsTheMeasuring) {
    Rig rig("gpu-no-source");
    rig.deps.gpuUse = nullptr;
    ScanSaysReadable(rig);
    ews::ServiceCore core(rig.deps, ews::ServiceSettings{rig.data.wide(), 60});
    CHECK(core.Start() == ews::StartResult::Ok);
    rig.clock.Advance(60);
    core.Tick();
    CHECK(rig.diag.CountStarting("{\"t\":\"tick\"") == 1);
    CHECK(Has(TickLine(rig, 0), "\"gpu\":{\"from\":\"default\"}"));
    core.Stop();
}

EWS_TEST(ServiceGpuAFailedReadCountsAsZeroAndTheMeasuringGoesOn) {
    Rig rig("gpu-fail");
    UseGraphicsModel(rig);
    ScanSaysReadable(rig);
    rig.gpuUse.script = {ScriptedGpuUse::Unavailable(), ScriptedGpuUse::Use(40.0), ScriptedGpuUse::Failed(),
                         ScriptedGpuUse::Use(60.0),     ScriptedGpuUse::Good({}),  ScriptedGpuUse::Use(20.0)};
    StartOk(rig);
    rig.Ticks(5);
    CHECK(rig.diag.CountStarting("{\"t\":\"tick\"") == 5);
    CHECK(Has(TickLine(rig, 0), "\"gpu\":{\"pct\":40.0,\"from\":\"service\"}"));
    CHECK(Has(TickLine(rig, 1), "\"gpu\":{\"from\":\"default\"}"));  // The read failed.
    CHECK(Has(TickLine(rig, 2), "\"gpu\":{\"pct\":60.0,\"from\":\"service\"}"));
    CHECK(Has(TickLine(rig, 3), "\"gpu\":{\"from\":\"default\"}"));  // No adapter.
    CHECK(Has(TickLine(rig, 4), "\"gpu\":{\"pct\":20.0,\"from\":\"service\"}"));
    // The report of the five minutes was still built and sent: 5 x 559 200 000 + 0.03 x (40 + 60 + 20) W x 60 s = 3 012 000 000 uJ.
    CHECK(rig.senders.batches.size() == 1);
    CHECK(Has(rig.SentLine(0, 0), "\"device_energy_uj\":3012000000"));
}

EWS_TEST(ServiceGpuAnAdapterWithNoThreeDInstanceIsUnknown) {
    Rig rig("gpu-copy-only");
    UseGraphicsModel(rig);
    ScanSaysReadable(rig);
    rig.gpuUse.script = {ScriptedGpuUse::Unavailable(), ScriptedGpuUse::Good({InstanceOf(1, "0_engtype_Copy", 30.0)})};
    StartOk(rig);
    rig.Ticks(1);
    CHECK(Has(TickLine(rig, 0), "\"gpu\":{\"from\":\"default\"}"));
}

EWS_TEST(ServiceGpuTheHighestAdapterIsTheUseOfTheMinute) {
    Rig rig("gpu-highest");
    UseGraphicsModel(rig);
    ScanSaysReadable(rig);
    rig.gpuUse.script = {ScriptedGpuUse::Unavailable(), ScriptedGpuUse::Good({ScriptedGpuUse::Engine(1, 20.0), ScriptedGpuUse::Engine(2, 50.0)})};
    StartOk(rig);
    rig.Ticks(1);
    CHECK(Has(TickLine(rig, 0), "\"gpu\":{\"pct\":50.0,\"from\":\"service\"}"));
    CHECK(Has(TickLine(rig, 0), "\"energy_uj\":649200000"));
}

EWS_TEST(ServiceGpuTheReadAtTheResumeIsNotTheUseOfAMinute) {
    Rig rig("gpu-resume");
    UseGraphicsModel(rig);
    ScanSaysReadable(rig);
    // Reads: 0 start, 1 first tick, 2 suspend, 3 resume (the sleep is in it), 4 second tick.
    rig.gpuUse.script = {ScriptedGpuUse::Unavailable(), ScriptedGpuUse::Use(50.0), ScriptedGpuUse::Use(20.0), ScriptedGpuUse::Use(99.0),
                         ScriptedGpuUse::Use(10.0)};
    StartOk(rig);
    rig.Ticks(1);
    rig.core.Suspend();
    rig.clock.Advance(3600);
    rig.core.Resume();
    rig.Ticks(1);
    CHECK(rig.gpuUse.reads == 5);
    CHECK(rig.diag.CountStarting("{\"t\":\"tick\"") == 2);
    CHECK(Has(TickLine(rig, 0), "\"gpu\":{\"pct\":50.0,\"from\":\"service\"}"));
    CHECK(Has(TickLine(rig, 1), "\"gpu\":{\"pct\":10.0,\"from\":\"service\"}"));
    CHECK(Has(TickLine(rig, 1), "\"flags\":[\"first_read\"]"));
    CHECK(rig.diag.CountContaining("99.0") == 0);
}

EWS_TEST(ServiceGpuAModelOfSchema2AddsNothingEvenWithAKnownUse) {
    Rig rig("gpu-schema2");
    ScanSaysReadable(rig);
    rig.gpuUse.script = {ScriptedGpuUse::Unavailable(), ScriptedGpuUse::Use(80.0)};
    StartOk(rig);
    rig.Ticks(1);
    std::string line = TickLine(rig, 0);
    CHECK(Has(line, "\"gpu\":{\"pct\":80.0,\"from\":\"service\"}"));
    CHECK(Has(line, "\"gpu_term_w\":0.000"));
    CHECK(Has(line, "\"device_w\":9.320,\"energy_uj\":559200000"));
}

EWS_TEST(ServiceGpuLevelL3UsesGpuWPerPct) {
    Rig rig("gpu-l3");
    UseGraphicsModel(rig);
    ScanSaysReadable(rig);
    rig.gpuUse.script = {ScriptedGpuUse::Unavailable(), ScriptedGpuUse::Use(50.0)};
    StartOk(rig);
    rig.emi.failRead = true;
    rig.Ticks(1);
    // L3: (3.48 + 0.075 x 20 + 1.40 + 0.04 x 50) W = 8.38 W x 60 s = 502.8 J.
    std::string line = TickLine(rig, 0);
    CHECK(Has(line, "\"level\":\"L3\""));
    CHECK(Has(line, "\"gpu_term_w\":2.000"));
    CHECK(Has(line, "\"energy_uj\":502800000"));
}

EWS_TEST(ServiceGpuModelLoadedEventShowsTheSchemaOfTheFile) {
    Rig rig("gpu-event");
    UseGraphicsModel(rig);
    StartOk(rig);
    CHECK(rig.diag.CountContaining("\"what\":\"model_loaded\",\"schema\":3,\"generated_utc\":\"2026-11-12T17:08:31Z\"}") == 1);
}

EWS_TEST(ServiceGpuAnInvalidSchema3ModelIsNoModel) {
    Rig rig("gpu-bad3");
    std::string text = ews_test::SpecModelText3();
    text.replace(text.find("\"rest_w_per_gpu_pct\": 0.03"), 26, "\"rest_w_per_gpu_pct\": 9");
    ews_test::WriteText(rig.data.path() / "calibration" / "model.json", text);
    StartOk(rig);
    CHECK(rig.diag.CountContaining("\"what\":\"model_invalid\"}") == 1);
    CHECK(Has(rig.Status(), "\"level\":\"L4\""));
}
