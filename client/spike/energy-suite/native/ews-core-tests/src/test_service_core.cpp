// test_service_core.cpp: tests of ServiceCore (the loop of the service) with fakes and a temporary data folder.
// Each test makes its own folder `ews-svc-test-*` below the system temporary folder and the rig removes it at the end.
// The default rig: 5 W package, 0.5 W memory, 20 percent processor load, the model of SPEC 8, no battery, 60 second
// ticks, a report every 5 ticks. One interval of 300 s on mains with an unknown brightness (counts as 50) gives
// 5 x (300 + 30 + 229.2) J = 2796 J of device energy.
#include <chrono>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

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

EWS_TEST(ServiceStatusHoldsTheSettingsInUse) {
    Rig rig("statussettings");
    rig.WriteIni("server_enabled=1\nserver_url=https://example.org:8443\ninterval_seconds=600\n"
                 "idle_threshold_seconds=120\nunused_threshold_seconds=900\ndevice_type=desktop\n");
    StartOk(rig);
    CHECK(Has(rig.Status(), "\"server_url\":\"https://example.org:8443\",\"server_key_set\":true,\"server_enabled\":true,"
                            "\"device_type\":\"desktop\",\"interval_seconds\":600,\"idle_threshold_seconds\":120,"
                            "\"unused_threshold_seconds\":900}"));
    CHECK(!Has(rig.Status(), "test-key"));
}

EWS_TEST(ServiceStatusWithSendingDisabledStillShowsUrlAndKey) {
    Rig rig("statusoff");
    rig.WriteIni("server_enabled=0\nserver_url=http://localhost:1\n");
    StartOk(rig);
    CHECK(Has(rig.Status(), "\"server_url\":\"http://localhost:1\",\"server_key_set\":true,\"server_enabled\":false"));
}

EWS_TEST(ServiceStatusWithoutKeyFileSaysKeyNotSet) {
    Rig rig("statusnokey");
    std::filesystem::remove_all(rig.data.path() / "config" / "key.bin");
    StartOk(rig);
    CHECK(Has(rig.Status(), "\"server_key_set\":false"));
}

EWS_TEST(ServiceStatusWithUndecryptableKeySaysKeyNotSet) {
    Rig rig("statusbadkey");
    ews_test::WriteText(rig.data.path() / "config" / "key.bin", "not a protected blob");
    StartOk(rig);
    CHECK(Has(rig.Status(), "\"server_key_set\":false"));
}

EWS_TEST(ServiceStatusWithBadUrlHasEmptyUrl) {
    Rig rig("statusbadurl");
    rig.WriteIni("server_enabled=1\nserver_url=ftp://nope\n");
    StartOk(rig);
    CHECK(Has(rig.Status(), "\"server_url\":\"\""));
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

namespace {

/// Gives the fake battery one cell of the given charge in mWh.
void SetCharge(Rig& rig, std::uint32_t mwh) {
    ews::BatteryState state = ews_test::MakeMwhBattery(true);
    state.remaining = mwh;
    state.remainingKnown = true;
    rig.battery.result = ews::Reading<std::vector<ews::BatteryState>>::Success({state});
}

/// @return How many times a part occurs in a text.
int Occurrences(const std::string& text, const std::string& part) {
    int count = 0;
    std::size_t at = text.find(part);
    while (at != std::string::npos) {
        ++count;
        at = text.find(part, at + 1);
    }
    return count;
}

}  // namespace

EWS_TEST(ServiceBatteryJumpAfterUnplugGivesAnEmptyBatteryFieldAndOneLogLine) {
    Rig rig("jump-unplug");
    SetCharge(rig, 35819);
    StartOk(rig);
    rig.Ticks(1);
    rig.clock.Advance(6);
    rig.power.Set(true);
    SetCharge(rig, 36491);
    rig.core.PowerSourceChanged();
    // Two history rows: the full minute and the short one of 6 seconds, whose battery field is empty.
    CHECK(CountLines(rig.History()) == 3);
    CHECK(Occurrences(rig.History(), ",,L2\n") == 1);
    CHECK(Occurrences(rig.Log(), " battery_jump 1\n") == 1);
}

EWS_TEST(ServiceJumpInTheMiddleOfAnIntervalIsReplacedAndTheReportKeepsBatteryFields) {
    Rig rig("jump-interval");
    rig.power.Set(true);
    SetCharge(rig, 30000);
    StartOk(rig);
    SetCharge(rig, 29989);
    rig.Ticks(1);
    SetCharge(rig, 25989);  // 4000 mWh in 60 s is 240 W: impossible
    rig.Ticks(1);
    SetCharge(rig, 25978);
    rig.Ticks(1);
    SetCharge(rig, 25967);
    rig.Ticks(1);
    SetCharge(rig, 25956);
    rig.Ticks(1);
    CHECK(rig.senders.batches.size() == 1);
    // Minute 2 takes the power of minute 3 (-11 mWh in 60 s = -0.66 W): five minutes of -11 mWh = -198 000 000 uJ.
    std::string report = rig.SentLine(0, 0);
    CHECK(Has(report, "\"energy_uj\":0"));
    CHECK(Has(report, "\"battery_energy_uj\":-198000000"));
    CHECK(Has(report, "\"device_energy_uj\":198000000"));
    CHECK(CountLines(rig.History()) == 6);
    CHECK(Occurrences(rig.History(), ",-0.660,L2\n") == 5);
    CHECK(Occurrences(rig.Log(), " battery_jump 1\n") == 1);
    CHECK(Occurrences(rig.Log(), " battery_replaced 1\n") == 1);
}

EWS_TEST(ServiceJumpInTheLastMinuteBeforeTheReportStaysUnknown) {
    Rig rig("jump-last");
    rig.power.Set(true);
    SetCharge(rig, 30000);
    StartOk(rig);
    SetCharge(rig, 29989);
    rig.Ticks(1);
    SetCharge(rig, 29978);
    rig.Ticks(1);
    SetCharge(rig, 29967);
    rig.Ticks(1);
    SetCharge(rig, 29956);
    rig.Ticks(1);
    SetCharge(rig, 25956);  // the fifth minute jumps
    rig.Ticks(1);
    std::string report = rig.SentLine(0, 0);
    CHECK(Has(report, "\"energy_uj\":0"));
    CHECK(!Has(report, "battery_energy_uj"));
    CHECK(!Has(report, "device_energy_uj"));
    CHECK(CountLines(rig.History()) == 6);
    CHECK(Occurrences(rig.History(), ",,L2\n") == 1);
    CHECK(Occurrences(rig.Log(), " battery_jump 1\n") == 1);
    CHECK(Occurrences(rig.Log(), " battery_replaced") == 0);
}

EWS_TEST(ServiceJumpAfterAPowerChangeWritesTheShortRowLateWithTheReplacedValue) {
    Rig rig("jump-late");
    SetCharge(rig, 35819);
    StartOk(rig);
    rig.Ticks(1);
    rig.clock.Advance(6);
    rig.power.Set(true);
    rig.core.PowerSourceChanged();  // ends the mains interval; the new interval is on battery
    rig.clock.Advance(6);
    SetCharge(rig, 36491);  // the gauge jumps up by 672 mWh in 6 s
    rig.core.Tick();
    SetCharge(rig, 36480);
    rig.Ticks(1);  // -11 mWh in 60 s = -0.66 W
    // Rows: header, full mains minute, 6 s mains row, the held 6 s row (late), the 60 s row.
    CHECK(CountLines(rig.History()) == 5);
    CHECK(Occurrences(rig.History(), ",-0.660,L2\n") == 2);
    CHECK(Occurrences(rig.History(), ",,L2\n") == 0);
    rig.core.Stop();  // closes the interval, which writes the counters to the log
    CHECK(Occurrences(rig.Log(), " battery_jump 1\n") == 1);
    CHECK(Occurrences(rig.Log(), " battery_replaced 1\n") == 1);
}

namespace {

/// Splits a text at one character.
/// @param text The text.
/// @param separator The character to split at.
/// @return The parts; a trailing empty part is kept.
std::vector<std::string> SplitAt(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::string part;
    std::istringstream stream(text);
    while (std::getline(stream, part, separator)) {
        parts.push_back(part);
    }
    return parts;
}

/// Reads one field of one history row (row 0 is the header line).
/// @param history The whole history text.
/// @param row The line number.
/// @param column The column number (device_w is 7, battery_w is 9).
/// @return The field text, or "<missing>" when the row or column does not exist.
std::string HistoryField(const std::string& history, std::size_t row, std::size_t column) {
    std::vector<std::string> lines = SplitAt(history, '\n');
    if (row >= lines.size()) {
        return "<missing>";
    }
    // A trailing empty field is lost by getline, so one comma is added to keep it.
    std::vector<std::string> fields = SplitAt(lines[row] + ",", ',');
    return column < fields.size() ? fields[column] : "<missing>";
}

}  // namespace

EWS_TEST(ServiceDeviceWOnBatteryIsTheBatteryPowerWithTheSignTurned) {
    Rig rig("devw-battery");
    rig.power.Set(true);
    SetCharge(rig, 30000);
    StartOk(rig);
    SetCharge(rig, 29909);  // -91 mWh in 60 s = -5.460 W
    rig.Ticks(1);
    CHECK(HistoryField(rig.History(), 1, 9) == "-5.460");
    CHECK(HistoryField(rig.History(), 1, 7) == "5.460");
}

EWS_TEST(ServiceDeviceWOnBatteryWithAnUnknownChangeIsEmpty) {
    Rig rig("devw-unknown");
    rig.power.Set(true);
    StartOk(rig);
    rig.Ticks(1);
    CHECK(HistoryField(rig.History(), 1, 9) == "");
    CHECK(HistoryField(rig.History(), 1, 7) == "");
}

EWS_TEST(ServiceDeviceWOfAReplacedMinuteIsTheReplacedPower) {
    Rig rig("devw-replaced");
    rig.power.Set(true);
    SetCharge(rig, 30000);
    StartOk(rig);
    SetCharge(rig, 29989);
    rig.Ticks(1);
    SetCharge(rig, 25989);  // impossible: held, then replaced by the next possible minute
    rig.Ticks(1);
    SetCharge(rig, 25978);
    rig.Ticks(1);
    CHECK(HistoryField(rig.History(), 2, 9) == "-0.660");
    CHECK(HistoryField(rig.History(), 2, 7) == "0.660");
    CHECK(HistoryField(rig.History(), 3, 7) == "0.660");
}

EWS_TEST(ServiceDeviceWOnMainsStaysThePowerOfTheMinute) {
    Rig rig("devw-mains");
    StartOk(rig);
    rig.Ticks(1);
    // 300 + 30 + 229.2 J in 60 s is not a battery value: the model and the counters give 9.320 W.
    CHECK(HistoryField(rig.History(), 1, 7) == "9.320");
}

EWS_TEST(ServiceDeviceWOnBatteryWithAZeroChangeIsHeldAndEmptyWhenTheServiceStops) {
    Rig rig("devw-zero");
    rig.power.Set(true);
    SetCharge(rig, 30000);
    StartOk(rig);
    rig.Ticks(1);  // the charge did not change: the row is held back (SPEC 7)
    CHECK(CountLines(rig.History()) == 0);
    rig.core.Stop();  // nothing fell before the stop: the held row is written with empty battery fields
    CHECK(HistoryField(rig.History(), 1, 9) == "");
    CHECK(HistoryField(rig.History(), 1, 7) == "");
}

EWS_TEST(ServiceStillMinuteSharesTheNextFallWithIt) {
    Rig rig("split-two");
    rig.power.Set(true);
    SetCharge(rig, 30000);
    StartOk(rig);
    rig.Ticks(1);
    SetCharge(rig, 29909);  // -91 mWh over the still minute and this one: 91 x 3.6 / 120 s = 2.730 W each
    rig.Ticks(1);
    CHECK(CountLines(rig.History()) == 3);
    CHECK(HistoryField(rig.History(), 1, 9) == "-2.730");
    CHECK(HistoryField(rig.History(), 1, 7) == "2.730");
    CHECK(HistoryField(rig.History(), 2, 9) == "-2.730");
    CHECK(HistoryField(rig.History(), 2, 7) == "2.730");
    CHECK(Occurrences(rig.Log(), " battery_split 1\n") == 1);
}

EWS_TEST(ServiceThreeStillMinutesShareAFallOfFortyMilliwattHours) {
    Rig rig("split-four");
    rig.power.Set(true);
    SetCharge(rig, 30000);
    StartOk(rig);
    rig.Ticks(3);
    SetCharge(rig, 29960);  // 40 x 3.6 / 240 s = 0.600 W for each of the four rows
    rig.Ticks(1);
    CHECK(CountLines(rig.History()) == 5);
    CHECK(Occurrences(rig.History(), ",-0.600,L2\n") == 4);
    CHECK(Occurrences(rig.Log(), " battery_split 3\n") == 1);
}

EWS_TEST(ServiceStillMinuteWhenThePowerSourceChangesIsWrittenEmpty) {
    Rig rig("split-source");
    rig.power.Set(true);
    SetCharge(rig, 30000);
    StartOk(rig);
    rig.Ticks(1);
    rig.clock.Advance(6);
    rig.power.Set(false);
    rig.core.PowerSourceChanged();
    // The 60 s row and the 6 s row were both still minutes on battery: both are written with empty battery fields.
    CHECK(CountLines(rig.History()) == 3);
    CHECK(HistoryField(rig.History(), 1, 9) == "");
    CHECK(HistoryField(rig.History(), 1, 7) == "");
    CHECK(HistoryField(rig.History(), 2, 9) == "");
    CHECK(HistoryField(rig.History(), 2, 7) == "");
    CHECK(Occurrences(rig.Log(), " battery_split") == 0);
}

EWS_TEST(ServiceStillMinutesHeldLongerThanFifteenMinutesAreWrittenEmpty) {
    Rig rig("split-long");
    rig.power.Set(true);
    SetCharge(rig, 30000);
    StartOk(rig);
    rig.Ticks(16);
    // Only the first minute has been held for more than 900 s.
    CHECK(CountLines(rig.History()) == 2);
    CHECK(HistoryField(rig.History(), 1, 9) == "");
    CHECK(HistoryField(rig.History(), 1, 7) == "");
}

EWS_TEST(ServiceJumpRowBehindAStillMinuteGetsTheSplitPower) {
    Rig rig("split-jump");
    rig.power.Set(true);
    SetCharge(rig, 30000);
    StartOk(rig);
    SetCharge(rig, 25000);  // impossible: held for replacement
    rig.Ticks(1);
    rig.Ticks(1);  // still
    SetCharge(rig, 24960);  // -40 mWh over the still minute and this one: 40 x 3.6 / 120 s = 1.200 W
    rig.Ticks(1);
    CHECK(CountLines(rig.History()) == 4);
    CHECK(Occurrences(rig.History(), ",-1.200,L2\n") == 3);
}

EWS_TEST(ServiceNormalFallWithNoHeldMinuteIsWrittenAtOnce) {
    Rig rig("split-none");
    rig.power.Set(true);
    SetCharge(rig, 30000);
    StartOk(rig);
    SetCharge(rig, 29989);
    rig.Ticks(1);
    CHECK(CountLines(rig.History()) == 2);
    CHECK(HistoryField(rig.History(), 1, 9) == "-0.660");
    CHECK(Occurrences(rig.Log(), " battery_split") == 0);
}

EWS_TEST(ServiceReportKeepsTheRawBatteryChangeWhenMinutesWereHeld) {
    Rig rig("split-report");
    rig.power.Set(true);
    SetCharge(rig, 30000);
    StartOk(rig);
    rig.Ticks(2);
    SetCharge(rig, 29960);
    rig.Ticks(1);
    SetCharge(rig, 29949);
    rig.Ticks(1);
    SetCharge(rig, 29938);
    rig.Ticks(1);
    // End minus start: 62 mWh = 223 200 000 uJ, whatever the history rows show.
    CHECK(rig.senders.batches.size() == 1);
    CHECK(Has(rig.SentLine(0, 0), "\"battery_energy_uj\":-223200000"));
    CHECK(CountLines(rig.History()) == 6);
}

EWS_TEST(ServiceStillMinutesStayHeldWhenAnIntervalEnds) {
    Rig rig("split-interval");
    rig.power.Set(true);
    SetCharge(rig, 30000);
    StartOk(rig);
    rig.Ticks(5);  // the interval ends with five held minutes
    CHECK(rig.senders.batches.size() == 1);
    CHECK(CountLines(rig.History()) == 0);
    SetCharge(rig, 29940);  // 60 x 3.6 / 360 s = 0.600 W for six rows
    rig.Ticks(1);
    CHECK(CountLines(rig.History()) == 7);
    CHECK(Occurrences(rig.History(), ",-0.600,L2\n") == 6);
}

EWS_TEST(ServiceMainsMinutesWithNoChangeAreNeverHeld) {
    Rig rig("split-mains");
    SetCharge(rig, 30000);
    StartOk(rig);
    rig.Ticks(1);
    CHECK(CountLines(rig.History()) == 2);
}

namespace {

/// Writes an old status.json (left by an earlier run of the service) before Start.
void WriteOldStatus(Rig& rig, const std::string& text) {
    ews_test::WriteText(rig.data.path() / "status" / "status.json", text);
}

/// @return An old status.json with the three kept fields as given (the clock of the rig starts at 10:00:00Z).
std::string OldStatus(const std::string& report, const std::string& send, const std::string& result) {
    return "{\"schema\":1,\"last_report_utc\":\"" + report + "\",\"last_send_utc\":\"" + send +
           "\",\"last_send_result\":\"" + result + "\",\"queued\":0}\n";
}

}  // namespace

EWS_TEST(ServiceRestartKeepsLastReportAndSend) {
    Rig rig("keepok");
    WriteOldStatus(rig, OldStatus("2026-11-14T09:55:00Z", "2026-11-14T09:55:01Z", "network"));
    StartOk(rig);
    CHECK(Has(rig.Status(), "\"last_report_utc\":\"2026-11-14T09:55:00Z\""));
    CHECK(Has(rig.Status(), "\"last_send_utc\":\"2026-11-14T09:55:01Z\""));
    CHECK(Has(rig.Status(), "\"last_send_result\":\"network\""));
}

EWS_TEST(ServiceRestartIgnoresBadValuesOneByOne) {
    Rig rig("keepbad");
    // The report time is in the future by more than 5 minutes, the send time is not a time, the word is unknown.
    WriteOldStatus(rig, OldStatus("2026-11-14T10:06:00Z", "yesterday", "fine"));
    StartOk(rig);
    CHECK(!Has(rig.Status(), "last_report_utc"));
    CHECK(!Has(rig.Status(), "last_send_utc"));
    CHECK(Has(rig.Status(), "\"last_send_result\":\"never\""));
}

EWS_TEST(ServiceRestartKeepsTheValidFieldsAmongBadOnes) {
    Rig rig("keepmixed");
    WriteOldStatus(rig, OldStatus("2026-11-14T10:05:00Z", "2026-13-45T99:99:99Z", "ok"));
    StartOk(rig);
    CHECK(Has(rig.Status(), "\"last_report_utc\":\"2026-11-14T10:05:00Z\""));
    CHECK(!Has(rig.Status(), "last_send_utc"));
    CHECK(Has(rig.Status(), "\"last_send_result\":\"ok\""));
}

EWS_TEST(ServiceRestartIgnoresBrokenOrHugeStatusFile) {
    Rig broken("keepbroken");
    WriteOldStatus(broken, "{\"last_report_utc\":\"2026-11-14T09:55:00Z\"");
    StartOk(broken);
    CHECK(!Has(broken.Status(), "last_report_utc"));
    Rig huge("keephuge");
    WriteOldStatus(huge, OldStatus("2026-11-14T09:55:00Z", "2026-11-14T09:55:01Z", "ok") + std::string(70 * 1024, ' '));
    StartOk(huge);
    CHECK(!Has(huge.Status(), "last_report_utc"));
    CHECK(Has(huge.Status(), "\"last_send_result\":\"never\""));
}

EWS_TEST(ServiceStartWithoutStatusFileGivesNoKeptValues) {
    Rig rig("keepnone");
    StartOk(rig);
    CHECK(!Has(rig.Status(), "last_report_utc"));
    CHECK(!Has(rig.Status(), "last_send_utc"));
    CHECK(Has(rig.Status(), "\"last_send_result\":\"never\""));
}

EWS_TEST(ServiceBackOffNeverWaitsMoreThanThirtyMinutes) {
    Rig rig("backoffcap");  // 300 s intervals: the cap is 6 intervals
    for (int i = 0; i < 6; ++i) {
        rig.senders.script.push_back(FakeSenders::Answer(ews::SendWord::Network, false));
    }
    StartOk(rig);
    // Failures wait 1, 2, 4, 6, 6 intervals; the sixth try comes after the series 1+2+4+6+6 skipped ticks.
    rig.Ticks(5);                           // try 1 fails (wait 1)
    rig.Ticks(5 * 2);                       // 1 skipped, try 2 fails (wait 2)
    rig.Ticks(5 * 3);                       // 2 skipped, try 3 fails (wait 4)
    rig.Ticks(5 * 5);                       // 4 skipped, try 4 fails (wait 6)
    rig.Ticks(5 * 7);                       // 6 skipped, try 5 fails (wait 6: the cap, not 8)
    CHECK(rig.senders.batches.size() == 5);
    rig.Ticks(5 * 7);                       // 6 skipped, try 6
    CHECK(rig.senders.batches.size() == 6);
}

EWS_TEST(ServiceNetworkBackWithAnEmptyQueueSendsNothing) {
    Rig rig("netempty");
    StartOk(rig);
    rig.core.NetworkBack();
    CHECK(rig.senders.batches.empty());
    CHECK(!Has(rig.Log(), "send_on_network"));
}

EWS_TEST(ServiceNetworkBackWithoutAServerSendsNothing) {
    Rig rig("netnoserver");
    rig.WriteIni("server_enabled=0\ninterval_seconds=300\ndevice_type=laptop\n");
    StartOk(rig);
    rig.Ticks(5);
    CHECK(CountLines(rig.Queue()) == 1);
    rig.core.NetworkBack();
    CHECK(rig.senders.batches.empty());
    CHECK(!Has(rig.Log(), "send_on_network"));
}

EWS_TEST(ServiceNetworkBackSendsAtOnceAndResetsTheBackOff) {
    Rig rig("netsend");
    rig.senders.script.push_back(FakeSenders::Answer(ews::SendWord::Network, false));
    rig.senders.script.push_back(FakeSenders::Answer(ews::SendWord::Network, false));
    StartOk(rig);
    rig.Ticks(5);  // try 1 fails, wait 1
    rig.Ticks(5);  // skipped
    CHECK(rig.senders.batches.size() == 1);
    rig.core.NetworkBack();  // try at once, fails again; the failure count restarts at 1 (wait 1, not 2)
    CHECK(rig.senders.batches.size() == 2);
    CHECK(Has(rig.Log(), " send_on_network"));
    CHECK(Has(rig.Status(), "\"queued\":2"));
    rig.Ticks(5);  // skipped
    CHECK(rig.senders.batches.size() == 2);
    rig.Ticks(5);  // sends all
    CHECK(rig.senders.batches.size() == 3);
    CHECK(rig.Queue().empty());
}

EWS_TEST(ServiceNetworkBackSuccessEmptiesTheQueueAndShowsIt) {
    Rig rig("netok");
    rig.senders.script.push_back(FakeSenders::Answer(ews::SendWord::Network, false));
    StartOk(rig);
    rig.Ticks(5);
    CHECK(Has(rig.Status(), "\"queued\":1"));
    rig.core.NetworkBack();
    CHECK(rig.Queue().empty());
    CHECK(Has(rig.Status(), "\"queued\":0"));
    CHECK(Has(rig.Status(), "\"last_send_result\":\"ok\""));
}

EWS_TEST(ServiceSendRequestSendsAtOnce) {
    Rig rig("reqsend");
    rig.senders.script.push_back(FakeSenders::Answer(ews::SendWord::Network, false));
    StartOk(rig);
    rig.core.SessionsChanged(true, false);
    rig.Ticks(5);
    CHECK(rig.senders.batches.size() == 1);
    rig.core.PipeMessage("EWS1 send\n", true);
    CHECK(rig.senders.batches.size() == 2);
    CHECK(rig.Queue().empty());
    CHECK(Has(rig.Log(), " send_requested"));
    CHECK(Has(rig.Status(), "\"queued\":0"));
}

EWS_TEST(ServiceSendRequestWithAnEmptyQueueSendsNothing) {
    Rig rig("reqempty");
    StartOk(rig);
    rig.core.PipeMessage("EWS1 send\n", true);
    CHECK(rig.senders.batches.empty());
    CHECK(!Has(rig.Log(), "send_requested"));
}

EWS_TEST(ServiceSendRequestFromASessionThatIsNotActiveIsDropped) {
    Rig rig("reqinactive");
    rig.senders.script.push_back(FakeSenders::Answer(ews::SendWord::Network, false));
    StartOk(rig);
    rig.Ticks(5);
    rig.core.PipeMessage("EWS1 send\n", false);
    CHECK(rig.senders.batches.size() == 1);
}

EWS_TEST(ServiceSendRequestIsLimitedToOneInThirtySeconds) {
    Rig rig("reqlimit");
    rig.senders.script.push_back(FakeSenders::Answer(ews::SendWord::Network, false));
    rig.senders.script.push_back(FakeSenders::Answer(ews::SendWord::Network, false));
    StartOk(rig);
    rig.Ticks(5);
    rig.core.PipeMessage("EWS1 send\n", true);  // try 2 fails
    CHECK(rig.senders.batches.size() == 2);
    rig.core.PipeMessage("EWS1 send\n", true);  // inside 30 s: dropped and counted
    CHECK(rig.senders.batches.size() == 2);
    rig.clock.Advance(31);
    rig.core.PipeMessage("EWS1 send\n", true);  // after 30 s: sends
    CHECK(rig.senders.batches.size() == 3);
    CHECK(rig.Queue().empty());
    rig.Ticks(5);
    CHECK(Has(rig.Log(), " pipe_dropped 1"));
}

EWS_TEST(ServiceJunkNextToTheSendCommandIsDroppedAndCounted) {
    Rig rig("reqjunk");
    rig.senders.script.push_back(FakeSenders::Answer(ews::SendWord::Network, false));
    StartOk(rig);
    rig.Ticks(5);
    rig.core.PipeMessage("EWS1 send", true);
    rig.core.PipeMessage("EWS1 send now\n", true);
    CHECK(rig.senders.batches.size() == 1);
    rig.Ticks(5);
    CHECK(Has(rig.Log(), " pipe_dropped 2"));
}

EWS_TEST(ServiceNoModelStillWritesTheMeasuredPartAndLevelL4InTheHistory) {
    Rig rig("measuredonly", false);
    StartOk(rig);
    rig.Ticks(5);
    // 5 W package + 0.5 W memory: device_w is empty, measured_w is 5.500, the level is L4.
    CHECK(Has(rig.History(), ",20.0,,5.500,,L4\n"));
    // The report is as before: no energy without a model.
    CHECK(!Has(rig.SentLine(0, 0), "energy_uj"));
}
