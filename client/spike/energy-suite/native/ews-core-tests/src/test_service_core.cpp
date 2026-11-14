// test_service_core.cpp: tests of ServiceCore (the loop of the service) with fakes and a temporary data folder.
// Each test makes its own folder `ews-svc-test-*` below the system temporary folder and the rig removes it at the end.
// The default rig: 5 W package, 0.5 W memory, 20 percent processor load, the model of SPEC 8, no battery, 60 second
// ticks, a report every 5 ticks. One interval of 300 s on mains with an unknown brightness (counts as 50) gives
// 5 x (300 + 30 + 229.2) J = 2796 J of device energy.
#include <chrono>
#include <filesystem>
#include <string>

#include "service_rig.h"
#include "test_common.h"

using ews_test::CountLines;
using ews_test::FakeSenders;
using ews_test::Has;
using ews_test::Rig;

namespace {

/// @return The default rig text of one full interval on mains.
const char* kFullIntervalDevice = "\"device_energy_uj\":2796000000";

/// Changes the model file so the rest power is 3 W higher, and moves its file time forward so the change is seen.
void RaiseRestPower(Rig& rig) {
    std::filesystem::path file = rig.data.path() / "calibration" / "model.json";
    std::string text = ews_test::SpecModelText();
    text.replace(text.find("\"rest_w\": 2.24"), 14, "\"rest_w\": 5.24");
    ews_test::WriteText(file, text);
    std::filesystem::last_write_time(file, std::filesystem::last_write_time(file) + std::chrono::seconds(100));
}

/// Starts the rig and checks that it started.
void StartOk(Rig& rig) { CHECK(rig.core.Start() == ews::StartResult::Ok); }

}  // namespace

EWS_TEST(ServiceFiveTicksGiveFiveRowsOneReportOneSend) {
    Rig rig("five");
    StartOk(rig);
    rig.Ticks(5);
    CHECK(CountLines(rig.History()) == 6);
    CHECK(Has(rig.History(), "v1,ts_utc,seconds,on_battery,state,idle_s,cpu_pct,device_w,measured_w,battery_w,level\n"));
    CHECK(rig.senders.batches.size() == 1);
    CHECK(rig.senders.batches.empty() || rig.senders.batches[0].size() == 1);
    CHECK(rig.Queue().empty());
    CHECK(Has(rig.Status(), "\"last_send_result\":\"ok\""));
    CHECK(Has(rig.Status(), "\"queued\":0"));
    CHECK(rig.senders.created == 1);
    CHECK(rig.scan.runs == 1);
}

EWS_TEST(ServiceReportHasTheRightFields) {
    Rig rig("fields");
    StartOk(rig);
    rig.Ticks(5);
    std::string report = rig.SentLine(0, 0);
    CHECK(Has(report, "\"timestamp\":\"2026-11-14T10:05:00Z\""));
    CHECK(Has(report, "\"interval_seconds\":300"));
    CHECK(Has(report, "\"cpu_percent\":20.0"));
    CHECK(Has(report, "\"on_battery\":false"));
    CHECK(Has(report, "\"os\":\"windows\",\"device_type\":\"laptop\""));
    CHECK(Has(report, "\"energy_uj\":2796000000"));
    CHECK(Has(report, kFullIntervalDevice));
    CHECK(!Has(report, "battery_energy_uj"));
    CHECK(Has(rig.Log(), " service_start"));
    CHECK(Has(rig.Log(), " send_ok 1 0"));
}

EWS_TEST(ServiceScanFilesAreWritten) {
    Rig rig("scanfiles");
    StartOk(rig);
    CHECK(Has(rig.data.Text("scan\\capabilities.service.json"), "\"context\": \"service\""));
    CHECK(!rig.data.Text("scan\\scan.service.log").empty());
}

EWS_TEST(ServiceSenderFailsThenBacksOffThenSendsAll) {
    Rig rig("backoff");
    rig.senders.script.push_back(FakeSenders::Answer(ews::SendWord::Network, false));
    StartOk(rig);
    rig.Ticks(5);
    CHECK(rig.senders.batches.size() == 1);
    CHECK(CountLines(rig.Queue()) == 1);
    CHECK(Has(rig.Status(), "\"last_send_result\":\"network\""));
    CHECK(Has(rig.Status(), "\"queued\":1"));
    rig.Ticks(5);
    CHECK(rig.senders.batches.size() == 1);  // the back-off skips this interval
    CHECK(CountLines(rig.Queue()) == 2);
    rig.Ticks(5);
    CHECK(rig.senders.batches.size() == 2);
    CHECK(rig.senders.batches.size() < 2 || rig.senders.batches[1].size() == 3);
    CHECK(rig.Queue().empty());
    CHECK(Has(rig.Status(), "\"last_send_result\":\"ok\""));
    CHECK(Has(rig.Log(), " send_network"));
}

EWS_TEST(ServiceUnauthorisedKeepsTheQueueAndRejectedDropsIt) {
    Rig rig("answers");
    rig.senders.script.push_back(FakeSenders::Answer(ews::SendWord::Unauthorised, false));
    StartOk(rig);
    rig.Ticks(5);
    CHECK(Has(rig.Status(), "\"last_send_result\":\"unauthorised\""));
    CHECK(CountLines(rig.Queue()) == 1);
    rig.Ticks(5);  // back-off interval
    rig.senders.script.push_back(FakeSenders::Answer(ews::SendWord::Rejected, true));
    rig.Ticks(5);
    CHECK(Has(rig.Status(), "\"last_send_result\":\"rejected\""));
    CHECK(rig.Queue().empty());
}

EWS_TEST(ServicePowerSourceChangeClosesTheIntervalEarly) {
    Rig rig("power");
    ews::BatteryState state = ews_test::MakeMwhBattery(true);
    state.remaining = 30000;
    state.remainingKnown = true;
    rig.battery.result = ews::Reading<std::vector<ews::BatteryState>>::Success({state});
    StartOk(rig);
    rig.Ticks(2);
    rig.clock.Advance(30);
    rig.power.Set(true);
    rig.core.PowerSourceChanged();
    CHECK(rig.senders.batches.size() == 1);
    CHECK(Has(rig.SentLine(0, 0), "\"interval_seconds\":150"));
    CHECK(Has(rig.SentLine(0, 0), "\"on_battery\":false"));
    CHECK(Has(rig.SentLine(0, 0), "\"battery_energy_uj\":0"));
    state.remaining = 29990;
    rig.battery.result = ews::Reading<std::vector<ews::BatteryState>>::Success({state});
    rig.Ticks(5);
    CHECK(rig.senders.batches.size() == 2);
    std::string second = rig.SentLine(1, 0);
    CHECK(Has(second, "\"interval_seconds\":300"));
    CHECK(Has(second, "\"on_battery\":true"));
    CHECK(Has(second, "\"energy_uj\":0"));
    CHECK(Has(second, "\"battery_energy_uj\":-36000000"));
    CHECK(Has(second, "\"device_energy_uj\":36000000"));
}

EWS_TEST(ServiceSameSourceReadingIsNotAChange) {
    Rig rig("samepower");
    StartOk(rig);
    rig.Ticks(2);
    rig.core.PowerSourceChanged();
    CHECK(rig.senders.batches.empty());
    CHECK(rig.Queue().empty());
}

EWS_TEST(ServiceSuspendQueuesWithoutSendingAndResumeStartsFresh) {
    Rig rig("suspend");
    StartOk(rig);
    rig.Ticks(2);
    rig.clock.Advance(30);
    rig.core.Suspend();
    CHECK(rig.senders.batches.empty());
    CHECK(CountLines(rig.Queue()) == 1);
    int rows = CountLines(rig.History());
    rig.clock.Advance(3600);
    rig.core.Tick();  // a tick during sleep is ignored
    CHECK(CountLines(rig.History()) == rows);
    rig.core.Resume();
    rig.Ticks(5);
    CHECK(rig.senders.batches.size() == 1);
    CHECK(rig.senders.batches.empty() || rig.senders.batches[0].size() == 2);
    CHECK(Has(rig.SentLine(0, 0), "\"interval_seconds\":150"));
    CHECK(Has(rig.SentLine(0, 1), "\"interval_seconds\":300"));
    CHECK(Has(rig.SentLine(0, 1), kFullIntervalDevice));
}

EWS_TEST(ServiceStopQueuesTheLastReportAndDoesNotSend) {
    Rig rig("stop");
    StartOk(rig);
    rig.Ticks(2);
    rig.clock.Advance(20);
    rig.core.Stop();
    CHECK(rig.senders.batches.empty());
    CHECK(CountLines(rig.Queue()) == 1);
    CHECK(Has(rig.Queue(), "\"interval_seconds\":140"));
    CHECK(Has(rig.Status(), "\"queued\":1"));
    CHECK(Has(rig.Log(), " service_stop"));
    ews::InstanceLock other;
    CHECK(other.Acquire(rig.data.wide()) == ews::LockState::Taken);
}

EWS_TEST(ServiceModelFileChangeIsUsedByTheNextInterval) {
    Rig rig("model");
    StartOk(rig);
    rig.Ticks(4);
    RaiseRestPower(rig);
    rig.Ticks(1);
    CHECK(Has(rig.SentLine(0, 0), kFullIntervalDevice));
    rig.Ticks(5);
    CHECK(rig.senders.batches.size() == 2);
    CHECK(Has(rig.SentLine(1, 0), "\"device_energy_uj\":3696000000"));
}

EWS_TEST(ServiceMissingModelStillReportsOnlyWhatIsKnown) {
    Rig rig("nomodel", false);
    StartOk(rig);
    rig.Ticks(5);
    CHECK(rig.senders.batches.size() == 1);
    CHECK(!Has(rig.SentLine(0, 0), "energy_uj"));
    CHECK(Has(rig.SentLine(0, 0), "\"cpu_percent\":20.0"));
}

EWS_TEST(ServiceInvalidModelIsCounted) {
    Rig rig("badmodel", false);
    ews_test::WriteText(rig.data.path() / "calibration" / "model.json", "{\"schema\":1}");
    StartOk(rig);
    CHECK(Has(rig.Log(), " model_invalid"));
}

EWS_TEST(ServiceUnknownProcessorPercentGivesNoReport) {
    Rig rig("nocpu");
    rig.cpu.fail = true;
    StartOk(rig);
    rig.Ticks(5);
    CHECK(rig.senders.batches.empty());
    CHECK(rig.Queue().empty());
    CHECK(Has(rig.Log(), " report_skipped 1"));
    CHECK(CountLines(rig.History()) == 6);
}

EWS_TEST(ServicePipeMessageFromAnActiveSessionSetsIdleAndState) {
    Rig rig("pipe");
    StartOk(rig);
    rig.core.SessionsChanged(true, false);
    rig.Ticks(4);
    rig.core.PipeMessage("EWS1 idle=5 bright=100\n", true);
    rig.Ticks(1);
    std::string report = rig.SentLine(0, 0);
    CHECK(Has(report, "\"idle_seconds\":65"));
    CHECK(Has(report, "\"state\":2"));
    CHECK(Has(report, "\"screen_locked\":false"));
    CHECK(Has(report, "\"device_energy_uj\":2827200000"));  // the last minute has brightness 100: +0.52 W
}

EWS_TEST(ServicePipeMessageFromASessionThatIsNotActiveIsIgnored) {
    Rig rig("pipeinactive");
    StartOk(rig);
    rig.core.SessionsChanged(true, false);
    rig.Ticks(4);
    rig.core.PipeMessage("EWS1 idle=5 bright=100\n", false);
    rig.Ticks(1);
    std::string report = rig.SentLine(0, 0);
    CHECK(Has(report, "\"idle_seconds\":300"));
    CHECK(Has(report, kFullIntervalDevice));
}

EWS_TEST(ServiceBadPipeMessageIsDroppedAndCounted) {
    Rig rig("pipebad");
    StartOk(rig);
    rig.core.SessionsChanged(true, false);
    rig.core.PipeMessage("EWS1 idle=-5 bright=50\n", true);
    rig.core.PipeMessage("hello", true);
    rig.Ticks(5);
    CHECK(Has(rig.SentLine(0, 0), "\"idle_seconds\":300"));
    CHECK(Has(rig.Log(), " pipe_dropped 2"));
}

EWS_TEST(ServiceNobodyLoggedOnGivesStateZeroAndLockedScreen) {
    Rig rig("nobody");
    StartOk(rig);
    rig.core.SessionsChanged(false, false);
    rig.core.PipeMessage("EWS1 idle=5 bright=50\n", true);
    rig.Ticks(5);
    CHECK(Has(rig.SentLine(0, 0), "\"state\":0"));
    CHECK(Has(rig.SentLine(0, 0), "\"screen_locked\":true"));
}

EWS_TEST(ServiceLockedSessionGivesLockedScreen) {
    Rig rig("locked");
    StartOk(rig);
    rig.core.SessionsChanged(true, true);
    rig.Ticks(5);
    CHECK(Has(rig.SentLine(0, 0), "\"screen_locked\":true"));
}

EWS_TEST(ServiceStartFailsWhenTheLockIsHeld) {
    Rig rig("lockheld");
    ews::InstanceLock holder;
    CHECK(holder.Acquire(rig.data.wide()) == ews::LockState::Taken);
    CHECK(rig.core.Start() == ews::StartResult::LockHeld);
    CHECK(Has(rig.Log(), " lock_held"));
    rig.Ticks(5);
    CHECK(rig.senders.batches.empty());
    CHECK(rig.History().empty());
}

EWS_TEST(ServiceStartFailsWithoutADataFolder) {
    Rig rig("nodata");
    std::filesystem::remove_all(rig.data.path());
    CHECK(rig.core.Start() == ews::StartResult::DataFolderMissing);
    CHECK(!std::filesystem::exists(rig.data.path()));
}

EWS_TEST(ServiceSendingOffQueuesWithoutSending) {
    Rig rig("sendingoff");
    rig.WriteIni("server_enabled=0\nserver_url=http://localhost:1\ninterval_seconds=300\ndevice_type=laptop\n");
    StartOk(rig);
    rig.Ticks(10);
    CHECK(rig.senders.batches.empty());
    CHECK(rig.senders.created == 0);
    CHECK(CountLines(rig.Queue()) == 2);
    CHECK(Has(rig.Status(), "\"last_send_result\":\"disabled\""));
    CHECK(Has(rig.Status(), "\"queued\":2"));
}

EWS_TEST(ServiceMissingKeyMeansNoSending) {
    Rig rig("nokey");
    std::filesystem::remove_all(rig.data.path() / "config" / "key.bin");
    StartOk(rig);
    rig.Ticks(5);
    CHECK(rig.senders.batches.empty());
    CHECK(CountLines(rig.Queue()) == 1);
    CHECK(Has(rig.Status(), "\"last_send_result\":\"disabled\""));
}

EWS_TEST(ServiceMissingIniMeansNoSending) {
    Rig rig("noini");
    std::filesystem::remove_all(rig.data.path() / "config" / "service.ini");
    StartOk(rig);
    rig.Ticks(5);
    CHECK(rig.senders.batches.empty());
    CHECK(CountLines(rig.Queue()) == 1);
    CHECK(Has(rig.Status(), "\"last_send_result\":\"disabled\""));
}

EWS_TEST(ServiceStatusAfterStartIsNever) {
    Rig rig("statusstart");
    StartOk(rig);
    CHECK(Has(rig.Status(), "\"last_send_result\":\"never\""));
    CHECK(Has(rig.Status(), "\"level\":\"L2\""));
    CHECK(Has(rig.Status(), "\"model_generated_utc\":\"2026-11-12T17:08:31Z\""));
}

EWS_TEST(ServiceLevelWithoutAModelIsL4) {
    Rig rig("levelnomodel", false);
    StartOk(rig);
    CHECK(Has(rig.Status(), "\"level\":\"L4\""));
    CHECK(!Has(rig.Status(), "model_generated_utc"));
}

EWS_TEST(ServiceFailedEmiReadFallsBackToTheModel) {
    Rig rig("emifail");
    StartOk(rig);
    rig.emi.failRead = true;
    rig.Ticks(5);
    CHECK(rig.senders.batches.size() == 1);
    // Level L3: (3.48 + 0.075 x 20 + 1.40) W x 300 s = 1914 J.
    CHECK(Has(rig.SentLine(0, 0), "\"device_energy_uj\":1914000000"));
}

EWS_TEST(ServiceLateTickAfterALongSleepIsAGap) {
    Rig rig("gap");
    StartOk(rig);
    rig.Ticks(2);
    rig.clock.Advance(7200);
    rig.core.Tick();
    CHECK(Has(rig.Log(), " gap_detected"));
    CHECK(rig.senders.batches.size() == 1);
    CHECK(Has(rig.SentLine(0, 0), "\"interval_seconds\":120"));
    CHECK(CountLines(rig.History()) == 3);
}

EWS_TEST(ServiceChangeOfDayDeletesOldHistoryFiles) {
    Rig rig("newday");
    ews_test::WriteText(rig.data.path() / "history" / "2026-11-01.csv", "v1\n");
    ews_test::WriteText(rig.data.path() / "history" / "2026-11-13.csv", "v1\n");
    StartOk(rig);
    CHECK(!std::filesystem::exists(rig.data.path() / "history" / "2026-11-01.csv"));
    CHECK(std::filesystem::exists(rig.data.path() / "history" / "2026-11-13.csv"));
    CHECK(Has(rig.Log(), " history_cleaned 1"));
}

EWS_TEST(ServiceDisplayOffLowersTheEnergy) {
    Rig rig("displayoff");
    StartOk(rig);
    rig.core.DisplayChanged(false);
    rig.Ticks(5);
    // Screen power is 0 instead of 1.40 W: 5 x 60 s x 1.40 W = 420 J less.
    CHECK(Has(rig.SentLine(0, 0), "\"device_energy_uj\":2376000000"));
}

EWS_TEST(ServiceBrightnessComesFromTheDisplayWhenTheScanSaysReadable) {
    Rig rig("displayread");
    rig.scan.result.display.common.readable = ews::Tri::True;
    rig.display.result = ews::Reading<ews::DisplayBrightness>::Success(ews::DisplayBrightness{100, 100});
    StartOk(rig);
    rig.Ticks(5);
    // Brightness 100 for every minute: +0.52 W x 300 s = 156 J.
    CHECK(Has(rig.SentLine(0, 0), "\"device_energy_uj\":2952000000"));
}
