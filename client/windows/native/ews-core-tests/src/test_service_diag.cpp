// test_service_diag.cpp: tests of the diagnostics file part of ServiceCore (SPEC 7a) with the rig of service_rig.h.
// The rig gives the core a fake writer, so the tests check which lines a tick, a report, a send and an event produce.
// The default rig: 5 W package and 0.5 W memory, 20 percent processor load, the model of SPEC 8, no battery, 60 second
// ticks, a report every 5 ticks, a ping every tick, a settings read at start (version 1). One minute on mains with an
// unknown brightness is 330 000 000 uJ measured and 229 200 000 uJ from the model: 559 200 000 uJ, 9.32 W.
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "service_rig.h"
#include "test_common.h"

using ews_test::FakeSenders;
using ews_test::Has;
using ews_test::Rig;

namespace {

/// Starts the rig and checks that it started.
void StartOk(Rig& rig) { CHECK(rig.core.Start() == ews::StartResult::Ok); }

/// @return True when the line is a tick line.
bool IsTickLine(const std::string& line) { return line.compare(0, 12, "{\"t\":\"tick\",") == 0; }

/// @param rig The rig. @param index 0 for the first tick line. @return That tick line, or an empty text.
std::string TickLine(const Rig& rig, std::size_t index) {
    std::vector<std::string> ticks;
    std::copy_if(rig.diag.lines.begin(), rig.diag.lines.end(), std::back_inserter(ticks), IsTickLine);
    return index < ticks.size() ? ticks[index] : std::string();
}

/// @return How many times the part is in the text.
int Occurrences(const std::string& text, const std::string& part) {
    int count = 0;
    std::size_t at = text.find(part);
    while (at != std::string::npos) {
        ++count;
        at = text.find(part, at + 1);
    }
    return count;
}

/// @return The position of the first written line that contains the part, or -1.
int IndexOf(const Rig& rig, const std::string& part) {
    int found = -1;
    for (std::size_t index = 0; index < rig.diag.lines.size(); ++index) {
        bool match = rig.diag.lines[index].find(part) != std::string::npos;
        found = (found < 0 && match) ? static_cast<int>(index) : found;
    }
    return found;
}

}  // namespace

EWS_TEST(DiagStartWritesTheStartModelAndSettingsEvents) {
    Rig rig("diag-start");
    StartOk(rig);
    const std::string start = std::string("\"what\":\"start\",\"service_version\":\"") + ews::kServiceVersion + "\"}";
    CHECK(rig.diag.CountContaining(start) == 1);
    CHECK(rig.diag.CountContaining("\"what\":\"model_loaded\",\"schema\":2,\"generated_utc\":\"2026-11-12T17:08:31Z\"}") == 1);
    CHECK(rig.diag.CountContaining("\"what\":\"level_change\",\"to\":\"L2\"}") == 1);
    CHECK(rig.diag.CountContaining("\"what\":\"settings_applied\",\"settings_version\":1,\"report_interval_seconds\":300,"
                                   "\"ping_interval_seconds\":60,\"idle_threshold_seconds\":600,\"unused_threshold_seconds\":3600}") == 1);
    CHECK(rig.diag.CountStarting("{\"t\":\"event\",\"ts\":\"2026-11-14T10:00:00Z\"") == static_cast<int>(rig.diag.lines.size()));
}

EWS_TEST(DiagMissingModelWritesNoModelEventAndAnInvalidOneWritesModelInvalid) {
    Rig missing("diag-nomodel", false);
    StartOk(missing);
    CHECK(missing.diag.CountContaining("model_loaded") == 0);
    CHECK(missing.diag.CountContaining("\"what\":\"level_change\",\"to\":\"L4\"}") == 1);
    Rig bad("diag-badmodel", false);
    ews_test::WriteText(bad.data.path() / "calibration" / "model.json", "{\"schema\":9}");
    StartOk(bad);
    CHECK(bad.diag.CountContaining("\"what\":\"model_invalid\"}") == 1);
}

EWS_TEST(DiagOneTickWritesOneTickLineWithEveryFieldTheServiceHas) {
    Rig rig("diag-tick");
    StartOk(rig);
    rig.Ticks(1);
    CHECK(rig.diag.CountStarting("{\"t\":\"tick\"") == 1);
    std::string line = TickLine(rig, 0);
    CHECK(line.rfind("{\"t\":\"tick\",\"ts\":\"2026-11-14T10:01:00Z\",\"seconds\":60,\"power\":{\"on_battery\":false},", 0) == 0);
    CHECK(Has(line, "{\"name\":\"RAPL_Package0_PKG\",\"kind\":\"cpu_package\",\"ok\":true,\"raw\":1166666666666,\"uj\":300000000}"));
    CHECK(Has(line, "{\"name\":\"RAPL_Package0_DRAM\",\"kind\":\"dram\",\"ok\":true,\"raw\":2016666666666,\"uj\":30000000}"));
    CHECK(Has(line, "\"battery\":[],\"cpu\":{\"pct\":20.0},"));
    CHECK(Has(line, "\"display\":{\"on\":true,\"brightness_pct\":50,\"brightness_from\":\"default\"},"));
    CHECK(Has(line, "\"session\":{\"logged_on\":false,\"locked\":false,\"idle_s\":60,\"state\":0},"));
    CHECK(Has(line, "\"calc\":{\"scan_level\":\"L2\",\"level\":\"L2\",\"model\":true,\"measured_uj\":330000000,\"rest_w\":2.240,"
                    "\"cpu_term_w\":0.180,\"screen_w\":1.400,\"device_w\":9.320,\"energy_uj\":559200000},"));
    CHECK(Has(line, "\"flags\":[\"first_read\"]}"));
}

EWS_TEST(DiagLaterTicksHaveNoFlags) {
    Rig rig("diag-flags");
    StartOk(rig);
    rig.Ticks(2);
    CHECK(rig.diag.CountStarting("{\"t\":\"tick\"") == 2);
    CHECK(Has(TickLine(rig, 1), "\"flags\":[]}"));
}

EWS_TEST(DiagTickShowsTheSessionAndThePipeMessage) {
    Rig rig("diag-session");
    StartOk(rig);
    rig.core.SessionsChanged(true, false);
    rig.core.PipeMessage("EWS1 idle=5 bright=70\n", true);
    rig.Ticks(1);
    std::string line = TickLine(rig, 0);
    CHECK(Has(line, "\"display\":{\"on\":true,\"brightness_pct\":70,\"brightness_from\":\"pipe\"}"));
    CHECK(Has(line, "\"session\":{\"logged_on\":true,\"locked\":false,\"idle_s\":65,\"idle_msg_age_s\":60,\"state\":2}"));
}

EWS_TEST(DiagTickShowsEveryBatteryAsRead) {
    Rig rig("diag-battery");
    ews::BatteryState state = ews_test::MakeMwhBattery(true);
    state.remaining = 30000;
    state.remainingKnown = true;
    rig.battery.result = ews::Reading<std::vector<ews::BatteryState>>::Success({state});
    StartOk(rig);
    rig.Ticks(1);
    CHECK(Has(TickLine(rig, 0), "\"battery\":[{\"ok\":true,\"unit\":\"mWh\",\"remaining_mwh\":30000,\"full_mwh\":36092,\"online\":false,\"charging\":false,\"discharging\":false}]"));
    CHECK(Has(TickLine(rig, 0), "\"battery_w\":0.000,\"energy_uj\":559200000}"));
}

EWS_TEST(DiagTickShowsAFailedBatteryRead) {
    Rig rig("diag-batteryfail");
    StartOk(rig);
    rig.battery.result = ews::Reading<std::vector<ews::BatteryState>>::Failure(ews::Status::Error, 5);
    rig.Ticks(1);
    std::string line = TickLine(rig, 0);
    CHECK(Has(line, "\"battery\":[{\"ok\":false}]"));
    CHECK(!Has(line, "energy_uj"));  // a battery is present but its change is unknown: SPEC 4.5 leaves the energy out
}

EWS_TEST(DiagFailedEmiReadHasNoRawAndNoEnergy) {
    Rig rig("diag-emifail");
    StartOk(rig);
    rig.emi.failRead = true;
    rig.Ticks(1);
    std::string line = TickLine(rig, 0);
    CHECK(Has(line, "\"emi\":[{\"name\":\"RAPL_Package0_PKG\",\"kind\":\"cpu_package\",\"ok\":false},{\"name\":\"RAPL_Package0_DRAM\",\"kind\":\"dram\",\"ok\":false}]"));
    CHECK(Has(line, "\"scan_level\":\"L2\",\"level\":\"L3\""));
}

EWS_TEST(DiagACounterThatWentBackIsFlaggedAndHasNoEnergy) {
    Rig rig("diag-back");
    StartOk(rig);
    rig.Ticks(1);
    rig.emi.counters[0] = 5;
    rig.emi.steps[0] = 1;
    rig.Ticks(1);
    std::string line = TickLine(rig, 1);
    CHECK(Has(line, "{\"name\":\"RAPL_Package0_PKG\",\"kind\":\"cpu_package\",\"ok\":true,\"raw\":6},"));
    CHECK(Has(line, "\"flags\":[\"counter_back\"]}"));
}

EWS_TEST(DiagAnImpossibleBatteryChangeIsFlaggedHeldAndThenReplaced) {
    Rig rig("diag-jump");
    rig.power.Set(true);
    ews::BatteryState state = ews_test::MakeMwhBattery(true);
    state.remaining = 30000;
    state.remainingKnown = true;
    rig.battery.result = ews::Reading<std::vector<ews::BatteryState>>::Success({state});
    StartOk(rig);
    state.remaining = 30100;  // the charge rose on battery: impossible
    rig.battery.result = ews::Reading<std::vector<ews::BatteryState>>::Success({state});
    rig.Ticks(1);
    CHECK(Has(TickLine(rig, 0), "\"flags\":[\"battery_jump\",\"held\",\"first_read\"]}"));
    state.remaining = 30090;  // a fall of 10 mWh in a minute: possible, and it replaces the held minute
    rig.battery.result = ews::Reading<std::vector<ews::BatteryState>>::Success({state});
    rig.Ticks(1);
    std::string second = TickLine(rig, 1);
    CHECK(Has(second, "\"flags\":[\"battery_replaced\"]}"));
    CHECK(Has(second, "\"battery_w\":-0.600,\"energy_uj\":0}"));
}

EWS_TEST(DiagFiveTicksWriteTheReportLineAndTheSendLine) {
    Rig rig("diag-report");
    ews::SendResult answer = FakeSenders::Answer(ews::SendWord::Ok, true);
    answer.httpStatus = 201;
    answer.created = 1;
    rig.senders.script.push_back(answer);
    StartOk(rig);
    rig.Ticks(5);
    const std::string expectedReport = "{\"t\":\"report\",\"ts\":\"2026-11-14T10:05:00Z\",\"queue\":1,\"report\":" + rig.SentLine(0, 0) + "}";
    CHECK(rig.diag.CountContaining(expectedReport) == 1);
    const std::string expectedSend = "{\"t\":\"send\",\"ts\":\"2026-11-14T10:05:00Z\",\"trigger\":\"tick\",\"reports\":1,\"result\":\"ok\","
                                     "\"http\":201,\"created\":1,\"duplicate\":0,\"rejected\":0,\"queue\":0}";
    CHECK(rig.diag.CountContaining(expectedSend) == 1);
    int report = IndexOf(rig, "{\"t\":\"report\"");
    CHECK(report > 0 && rig.diag.lines[static_cast<std::size_t>(report) - 1].compare(0, 12, "{\"t\":\"tick\",") == 0);
    CHECK(IndexOf(rig, "{\"t\":\"send\"") == report + 1);
}

EWS_TEST(DiagASendWithoutAnAnswerHasNoHttpAndTheQueueStays) {
    Rig rig("diag-sendfail");
    rig.senders.script.push_back(FakeSenders::Answer(ews::SendWord::Network, false));
    StartOk(rig);
    rig.Ticks(5);
    CHECK(rig.diag.CountContaining("{\"t\":\"send\",\"ts\":\"2026-11-14T10:05:00Z\",\"trigger\":\"tick\",\"reports\":1,\"result\":\"network\",\"queue\":1}") == 1);
}

EWS_TEST(DiagSendTriggerCommandAndNetwork) {
    Rig rig("diag-trigger-a");
    rig.senders.script.push_back(FakeSenders::Answer(ews::SendWord::Network, false));
    rig.senders.pingScript.push_back(FakeSenders::Reply(200, FakeSenders::PingJson(1, true, FakeSenders::CommandJson(7, "report_now"))));
    StartOk(rig);
    rig.Ticks(1);  // the ping of the first tick runs report_now; its send fails
    CHECK(rig.diag.CountContaining("\"trigger\":\"command\",\"reports\":1,\"result\":\"network\"") == 1);
    rig.core.NetworkBack();  // the queue holds that report; the next answer is ok
    CHECK(rig.diag.CountContaining("\"trigger\":\"network\",\"reports\":1,\"result\":\"ok\"") == 1);
}

EWS_TEST(DiagSendTriggerPipe) {
    Rig rig("diag-trigger-b");
    rig.senders.script.push_back(FakeSenders::Answer(ews::SendWord::Network, false));
    StartOk(rig);
    rig.Ticks(5);  // the report tick fails
    CHECK(rig.diag.CountContaining("\"trigger\":\"tick\",\"reports\":1,\"result\":\"network\"") == 1);
    rig.core.PipeMessage("EWS1 send\n", true);
    CHECK(rig.diag.CountContaining("\"trigger\":\"pipe\",\"reports\":1,\"result\":\"ok\"") == 1);
}

EWS_TEST(DiagACommandWritesAnEventWithFixedWordsOnly) {
    Rig rig("diag-command");
    rig.senders.pingScript.push_back(FakeSenders::Reply(200, FakeSenders::PingJson(1, true, FakeSenders::CommandJson(7, "report_now") + "," + FakeSenders::CommandJson(8, "format_disk"))));
    StartOk(rig);
    rig.Ticks(1);
    CHECK(rig.diag.CountContaining("\"what\":\"command\",\"type\":\"report_now\",\"result\":\"done\"}") == 1);
    CHECK(rig.diag.CountContaining("\"what\":\"command\",\"type\":\"unknown\",\"result\":\"unsupported\"}") == 1);
    CHECK(rig.diag.CountContaining("format_disk") == 0);
    CHECK(rig.diag.CountContaining("\"trigger\":\"command\"") == 1);
}

EWS_TEST(DiagPingEventOnlyWhenTheResultChanges) {
    Rig rig("diag-ping");
    rig.senders.pingScript.push_back(FakeSenders::Reply(401, "{}"));
    StartOk(rig);
    rig.Ticks(4);
    CHECK(rig.diag.CountContaining("\"what\":\"ping\"") == 2);
    CHECK(IndexOf(rig, "\"what\":\"ping\",\"result\":\"unauthorised\"}") < IndexOf(rig, "\"what\":\"ping\",\"result\":\"ok\"}"));
    CHECK(IndexOf(rig, "\"what\":\"ping\",\"result\":\"unauthorised\"}") > 0);
}

EWS_TEST(DiagPowerSourceDisplaySessionSuspendResumeAndStopEvents) {
    Rig rig("diag-events");
    StartOk(rig);
    rig.Ticks(2);
    rig.core.DisplayChanged(false);
    rig.core.SessionsChanged(true, true);
    rig.clock.Advance(30);
    rig.power.Set(true);
    rig.core.PowerSourceChanged();
    rig.clock.Advance(30);
    rig.core.Suspend();
    rig.clock.Advance(600);
    rig.core.Resume();
    rig.core.Stop();
    CHECK(rig.diag.CountContaining("\"what\":\"display\",\"on\":false}") == 1);
    CHECK(rig.diag.CountContaining("\"what\":\"session\",\"logged_on\":true,\"locked\":true}") == 1);
    CHECK(rig.diag.CountContaining("\"what\":\"power_source\",\"on_battery\":true}") == 1);
    CHECK(rig.diag.CountContaining("\"what\":\"suspend\"}") == 1);
    CHECK(rig.diag.CountContaining("\"what\":\"resume\"}") == 1);
    CHECK(rig.diag.CountContaining("\"what\":\"stop\"}") == 1);
}

EWS_TEST(DiagAResumeThenATickIsFlaggedFirstRead) {
    Rig rig("diag-resume");
    StartOk(rig);
    rig.Ticks(1);
    rig.clock.Advance(30);
    rig.core.Suspend();
    rig.clock.Advance(600);
    rig.core.Resume();
    rig.Ticks(1);
    std::string last = rig.diag.FirstStarting("{\"t\":\"tick\",\"ts\":\"2026-11-14T10:12:30Z\"");
    CHECK(Has(last, "\"flags\":[\"first_read\"]}"));
}

EWS_TEST(DiagAGapWritesAnEventAndFlagsTheNextMinute) {
    Rig rig("diag-gap");
    StartOk(rig);
    rig.Ticks(1);
    rig.clock.Advance(400);
    rig.core.Tick();
    CHECK(rig.diag.CountContaining("\"what\":\"gap\"}") == 1);
    CHECK(rig.diag.CountStarting("{\"t\":\"tick\"") == 1);  // the late tick calculates no minute
    rig.Ticks(1);
    CHECK(Has(TickLine(rig, 1), "\"flags\":[\"gap\"]}"));
}

EWS_TEST(DiagNothingSecretReachesTheFile) {
    Rig rig("diag-secret");
    StartOk(rig);
    rig.Ticks(5);
    CHECK(rig.diag.CountContaining("test-key") == 0);
    CHECK(rig.diag.CountContaining("localhost") == 0);
    CHECK(rig.diag.CountContaining("http://") == 0);
    CHECK(rig.diag.CountContaining("\\") == 0);  // no file path: no backslash at all
}

EWS_TEST(DiagAFailedWriteNeverStopsMeasuringAndIsLoggedOncePerHour) {
    Rig rig("diag-fail");
    rig.diag.next = ews::DiagWriteResult::Failed;
    StartOk(rig);
    rig.Ticks(5);
    CHECK(ews_test::CountLines(rig.History()) == 6);
    CHECK(rig.senders.batches.size() == 1);
    CHECK(Has(rig.Status(), "\"last_send_result\":\"ok\""));
    CHECK(Occurrences(rig.Log(), " diag_write_failed") == 1);
    CHECK(Has(rig.Log(), " diag_write_failed 1\n"));
    CHECK(rig.diag.lines.empty());
    rig.Ticks(60);
    CHECK(Occurrences(rig.Log(), " diag_write_failed") == 2);
}

EWS_TEST(DiagAFullFileIsLoggedOnceAndNothingMoreIsTriedThatDay) {
    Rig rig("diag-full");
    rig.diag.next = ews::DiagWriteResult::Full;
    StartOk(rig);
    rig.Ticks(5);
    CHECK(Occurrences(rig.Log(), " diag_full\n") == 1);
    CHECK(rig.diag.appends == 1);
    CHECK(ews_test::CountLines(rig.History()) == 6);
}

EWS_TEST(DiagTheFilesAreCleanedAtTheStartAndAtTheChangeOfDay) {
    Rig rig("diag-clean");
    StartOk(rig);
    CHECK(rig.diag.cleans == 1);
    rig.Ticks(3);
    CHECK(rig.diag.cleans == 1);
    rig.clock.Advance(24 * 3600);
    rig.core.Tick();  // a gap: no clean and no minute
    CHECK(rig.diag.cleans == 1);
    rig.Ticks(1);
    CHECK(rig.diag.cleans == 2);
}
