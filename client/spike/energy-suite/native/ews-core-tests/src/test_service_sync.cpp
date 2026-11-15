// test_service_sync.cpp: tests of the ping, the server settings, the commands and the registration of ServiceCore.
// The fake server (FakeSenders) records every call and answers from a script. The default rig: a report every 5 ticks,
// a ping every tick, a first settings read at start that gives the same values as service.ini (version 1).
#include <filesystem>
#include <string>
#include <vector>

#include "ews/device_identity.h"
#include "service_rig.h"
#include "test_common.h"

using ews_test::CountLines;
using ews_test::FakeSenders;
using ews_test::Has;
using ews_test::Rig;

namespace {

/// Starts the rig and checks that it started.
void StartOk(Rig& rig) { CHECK(rig.core.Start() == ews::StartResult::Ok); }

/// Puts the same answer in a script a number of times.
template <typename T>
void Repeat(std::vector<T>& script, const T& answer, int count) {
    for (int i = 0; i < count; ++i) {
        script.push_back(answer);
    }
}

/// @return The answer to a ping that lists these commands (JSON items), with the settings version 1.
ews::ApiAnswer WithCommands(const std::string& commands) { return FakeSenders::Reply(200, FakeSenders::PingJson(1, true, commands)); }

/// @return The device id of the rig.
std::string DeviceId(Rig& rig) { return ews::LoadOrCreateDeviceIdentity(rig.data.wide()).value.id; }

}  // namespace

EWS_TEST(SyncNoPingWithoutAnAddress) {
    Rig rig("sync-nourl");
    rig.WriteIni("server_enabled=1\ninterval_seconds=300\n");
    StartOk(rig);
    rig.Ticks(3);
    CHECK(rig.senders.calls.empty());
    CHECK(Has(rig.Status(), "\"last_ping_result\":\"never\""));
    CHECK(Has(rig.Status(), "\"settings_source\":\"local\""));
}

EWS_TEST(SyncNoPingWithoutAKey) {
    Rig rig("sync-nokey");
    std::filesystem::remove(rig.data.path() / "config" / "key.bin");
    StartOk(rig);
    rig.Ticks(3);
    CHECK(rig.senders.calls.empty());
}

EWS_TEST(SyncPingBodyIsExactWithoutAcks) {
    Rig rig("sync-body");
    StartOk(rig);
    rig.Ticks(1);
    std::string expected = "{\"device_id\":\"" + DeviceId(rig) + "\",\"unsent_reports\":0}";
    CHECK(rig.senders.CallsTo("/api/ping").size() == 1);
    CHECK(rig.senders.BodyOf("/api/ping", 0) == expected);
    CHECK(rig.senders.MethodOf("/api/ping", 0) == "POST");
}

EWS_TEST(SyncPingBodyCountsTheQueueAndCarriesAcks) {
    Rig rig("sync-acks");
    rig.senders.script.push_back(FakeSenders::Answer(ews::SendWord::Network, false));
    rig.senders.pingScript.push_back(FakeSenders::Reply(200, FakeSenders::PingJson(1, true, FakeSenders::CommandJson(41, "reboot"))));
    StartOk(rig);
    rig.Ticks(5);
    CHECK(Has(rig.senders.BodyOf("/api/ping", 4), "\"unsent_reports\":1}"));
    CHECK(Has(rig.senders.BodyOf("/api/ping", 1), "\"unsent_reports\":0,\"acks\":[{\"command_id\":41,\"result\":\"unsupported\"}]}"));
}

EWS_TEST(SyncPingIntervalBelow60PingsEveryTick) {
    Rig rig("sync-ping30");
    rig.senders.settingsDefault = FakeSenders::Reply(200, FakeSenders::SettingsJson(1, 300, 30, 600, 3600));
    StartOk(rig);
    rig.Ticks(6);
    CHECK(rig.senders.CallsTo("/api/ping").size() == 6);
}

EWS_TEST(SyncPingIntervalIsRoundedToMinutes) {
    Rig rig("sync-ping150");
    rig.senders.settingsDefault = FakeSenders::Reply(200, FakeSenders::SettingsJson(1, 300, 150, 600, 3600));
    StartOk(rig);
    rig.Ticks(6);
    // 150 seconds count as 3 minutes: the first tick pings, then every third tick.
    CHECK(rig.senders.CallsTo("/api/ping").size() == 2);
}

EWS_TEST(SyncSettingsAreReadAtStartAndStored) {
    Rig rig("sync-start");
    rig.senders.settingsDefault = FakeSenders::Reply(200, FakeSenders::SettingsJson(7, 600, 60, 300, 1800));
    StartOk(rig);
    CHECK(rig.senders.CallsTo("/api/settings").size() == 1);
    CHECK(rig.senders.MethodOf("/api/settings", 0) == "GET");
    std::string status = rig.Status();
    CHECK(Has(status, "\"interval_seconds\":600"));
    CHECK(Has(status, "\"idle_threshold_seconds\":300"));
    CHECK(Has(status, "\"unused_threshold_seconds\":1800"));
    CHECK(Has(status, "\"settings_source\":\"server\",\"settings_version\":7,\"last_settings_utc\":\"2026-11-14T10:00:00Z\""));
    CHECK(Has(rig.data.Text("status\\server-settings.json"), "\"settings_version\":7"));
    CHECK(Has(rig.Log(), " settings_applied 7"));
}

EWS_TEST(SyncSameVersionIsNotReadAgain) {
    Rig rig("sync-same");
    StartOk(rig);
    rig.Ticks(4);
    CHECK(rig.senders.CallsTo("/api/ping").size() == 4);
    CHECK(rig.senders.CallsTo("/api/settings").size() == 1);
}

EWS_TEST(SyncNewVersionIsReadAndAppliedWithoutARestart) {
    Rig rig("sync-newver");
    rig.senders.pingDefault = FakeSenders::Reply(200, FakeSenders::PingJson(2, true, ""));
    rig.senders.settingsScript = {FakeSenders::Reply(200, FakeSenders::SettingsJson(1, 300, 60, 600, 3600)),
                                  FakeSenders::Reply(200, FakeSenders::SettingsJson(2, 600, 60, 120, 900))};
    StartOk(rig);
    CHECK(Has(rig.Status(), "\"interval_seconds\":300"));
    rig.Ticks(1);
    CHECK(rig.senders.CallsTo("/api/settings").size() == 2);
    std::string status = rig.Status();
    CHECK(Has(status, "\"interval_seconds\":600,\"idle_threshold_seconds\":120,\"unused_threshold_seconds\":900"));
    CHECK(Has(status, "\"settings_version\":2"));
    rig.Ticks(4);
    CHECK(rig.senders.CallsTo("/api/settings").size() == 2);
    CHECK(rig.senders.batches.empty());
}

EWS_TEST(SyncShorterReportIntervalEndsTheRunningPeriodAtTheRightTick) {
    Rig rig("sync-shorter");
    rig.senders.pingScript.push_back(FakeSenders::Reply(200, FakeSenders::PingJson(2, true, "")));
    rig.senders.settingsScript = {FakeSenders::Reply(200, FakeSenders::SettingsJson(1, 300, 60, 600, 3600)),
                                  FakeSenders::Reply(200, FakeSenders::SettingsJson(2, 120, 60, 600, 3600))};
    StartOk(rig);
    rig.Ticks(1);
    CHECK(rig.senders.batches.empty());
    rig.Ticks(1);
    CHECK(rig.senders.batches.size() == 1);
    CHECK(Has(rig.SentLine(0, 0), "\"interval_seconds\":120"));
}

EWS_TEST(SyncAPeriodAlreadyLongerThanTheNewLengthEndsAtTheNextTick) {
    Rig rig("sync-longer");
    Repeat(rig.senders.pingScript, FakeSenders::Reply(200, FakeSenders::PingJson(1, true, "")), 7);
    rig.senders.pingScript.push_back(FakeSenders::Reply(200, FakeSenders::PingJson(2, true, "")));
    rig.senders.settingsScript = {FakeSenders::Reply(200, FakeSenders::SettingsJson(1, 900, 60, 600, 3600)),
                                  FakeSenders::Reply(200, FakeSenders::SettingsJson(2, 300, 60, 600, 3600))};
    StartOk(rig);
    rig.Ticks(8);
    CHECK(rig.senders.batches.empty());
    rig.Ticks(1);
    CHECK(rig.senders.batches.size() == 1);
    CHECK(Has(rig.SentLine(0, 0), "\"interval_seconds\":540"));
}

EWS_TEST(SyncBadSettingsAnswerIsIgnoredAndNotReadAgainForTheSameVersion) {
    Rig rig("sync-bad");
    Repeat(rig.senders.pingScript, FakeSenders::Reply(200, FakeSenders::PingJson(2, true, "")), 3);
    rig.senders.settingsScript = {FakeSenders::Reply(200, FakeSenders::SettingsJson(1, 300, 60, 600, 3600)),
                                  FakeSenders::Reply(200, FakeSenders::SettingsJson(2, 900, 60, 600, 600))};
    StartOk(rig);
    rig.Ticks(3);
    CHECK(rig.senders.CallsTo("/api/settings").size() == 2);
    CHECK(Has(rig.Status(), "\"interval_seconds\":300"));
    CHECK(Has(rig.Status(), "\"settings_version\":1"));
    CHECK(Has(rig.Log(), " settings_ignored 1\n"));
}

EWS_TEST(SyncFailedPingKeepsTheSettingsAndTheReports) {
    Rig rig("sync-401");
    rig.senders.settingsDefault = FakeSenders::Reply(200, FakeSenders::SettingsJson(3, 300, 60, 120, 900));
    Repeat(rig.senders.pingScript, FakeSenders::Reply(401, "{}"), 5);
    StartOk(rig);
    rig.Ticks(5);
    CHECK(Has(rig.Status(), "\"idle_threshold_seconds\":120"));
    CHECK(Has(rig.Status(), "\"last_ping_result\":\"unauthorised\""));
    CHECK(Has(rig.Log(), " ping_failed 1 1\n"));
    CHECK(rig.senders.batches.size() == 1);
}

EWS_TEST(SyncNetworkAndTimeoutKeepTheSettings) {
    Rig rig("sync-net");
    rig.senders.settingsDefault = FakeSenders::Reply(200, FakeSenders::SettingsJson(3, 300, 60, 120, 900));
    Repeat(rig.senders.pingScript, FakeSenders::Reply(0, ""), 3);
    StartOk(rig);
    rig.Ticks(3);
    CHECK(Has(rig.Status(), "\"last_ping_result\":\"network\""));
    CHECK(Has(rig.Status(), "\"idle_threshold_seconds\":120"));
    CHECK(Has(rig.Log(), " ping_failed 4 1\n"));
}

EWS_TEST(SyncRejectedPingIsCountedAndNothingStops) {
    Rig rig("sync-422");
    rig.senders.pingScript.push_back(FakeSenders::Reply(422, "{}"));
    StartOk(rig);
    rig.Ticks(5);
    CHECK(Has(rig.Log(), " ping_failed 5 1\n"));
    CHECK(Has(rig.Status(), "\"last_ping_result\":\"ok\""));
    CHECK(rig.senders.batches.size() == 1);
}

EWS_TEST(SyncRateLimitedPingWaitsForRetryAfter) {
    Rig rig("sync-429");
    rig.senders.pingScript.push_back(FakeSenders::Reply(429, "{}", 120));
    StartOk(rig);
    rig.Ticks(1);
    CHECK(Has(rig.Status(), "\"last_ping_result\":\"rate_limited\""));
    rig.Ticks(1);
    CHECK(rig.senders.CallsTo("/api/ping").size() == 1);
    rig.Ticks(1);
    CHECK(rig.senders.CallsTo("/api/ping").size() == 2);
}

EWS_TEST(SyncRetryAfterOfAnHourIsTheMostThatIsHonoured) {
    Rig rig("sync-503");
    rig.senders.pingScript.push_back(FakeSenders::Reply(503, "{}", 7200));
    StartOk(rig);
    rig.Ticks(59);
    CHECK(rig.senders.CallsTo("/api/ping").size() == 1);
    CHECK(Has(rig.Status(), "\"last_ping_result\":\"server_error\""));
    rig.Ticks(1);
    CHECK(rig.senders.CallsTo("/api/ping").size() == 1);
    rig.Ticks(1);
    CHECK(rig.senders.CallsTo("/api/ping").size() == 2);
}

EWS_TEST(SyncFailedSettingsReadAtStartKeepsTheLocalSettings) {
    Rig rig("sync-offline");
    rig.senders.settingsScript.push_back(FakeSenders::Reply(0, ""));
    StartOk(rig);
    CHECK(Has(rig.Status(), "\"settings_source\":\"local\""));
    CHECK(Has(rig.Status(), "\"interval_seconds\":300"));
    CHECK(!Has(rig.Status(), "last_settings_utc"));
    CHECK(Has(rig.Log(), " settings_failed 4\n"));
}

EWS_TEST(SyncSettingsFileSurvivesARestartAndWorksOffline) {
    Rig rig("sync-restart");
    rig.senders.settingsDefault = FakeSenders::Reply(200, FakeSenders::SettingsJson(5, 600, 120, 300, 1800));
    StartOk(rig);
    rig.core.Stop();
    rig.senders.settingsDefault = FakeSenders::Reply(0, "");
    ews::ServiceCore second(rig.deps, ews::ServiceSettings{rig.data.wide(), 60});
    CHECK(second.Start() == ews::StartResult::Ok);
    std::string status = rig.Status();
    CHECK(Has(status, "\"interval_seconds\":600,\"idle_threshold_seconds\":300,\"unused_threshold_seconds\":1800"));
    CHECK(Has(status, "\"settings_source\":\"server\",\"settings_version\":5"));
    second.Stop();
}

EWS_TEST(SyncBrokenSettingsFileIsIgnoredAndCounted) {
    Rig rig("sync-brokenfile");
    ews_test::WriteText(rig.data.path() / "status" / "server-settings.json", "{\"report_interval_seconds\":5}\n");
    rig.senders.settingsScript.push_back(FakeSenders::Reply(0, ""));
    StartOk(rig);
    CHECK(Has(rig.Status(), "\"settings_source\":\"local\""));
    CHECK(Has(rig.Status(), "\"interval_seconds\":300"));
    CHECK(Has(rig.Log(), " settings_ignored 1\n"));
}

EWS_TEST(SyncSetIntervalChangesTheReportIntervalAndIsAcked) {
    Rig rig("sync-setint");
    rig.senders.pingScript.push_back(WithCommands(FakeSenders::CommandJson(11, "set_interval", "{\"report_interval_seconds\":120}")));
    StartOk(rig);
    rig.Ticks(1);
    CHECK(Has(rig.Status(), "\"interval_seconds\":120"));
    CHECK(Has(rig.data.Text("status\\server-settings.json"), "\"report_interval_seconds\":120"));
    rig.Ticks(1);
    CHECK(rig.senders.batches.size() == 1);
    CHECK(Has(rig.senders.BodyOf("/api/ping", 1), "\"acks\":[{\"command_id\":11,\"result\":\"done\"}]"));
}

EWS_TEST(SyncSetIntervalWithBadParamsFails) {
    Rig rig("sync-setbad");
    std::string commands = FakeSenders::CommandJson(11, "set_interval", "{\"report_interval_seconds\":5}") + "," +
                           FakeSenders::CommandJson(12, "set_interval", "{}") + "," +
                           FakeSenders::CommandJson(13, "set_interval", "{\"report_interval_seconds\":\"abc\"}") + "," +
                           FakeSenders::CommandJson(14, "set_interval", "{\"report_interval_seconds\":99999}");
    rig.senders.pingScript.push_back(WithCommands(commands));
    StartOk(rig);
    rig.Ticks(2);
    CHECK(Has(rig.Status(), "\"interval_seconds\":300"));
    std::string body = rig.senders.BodyOf("/api/ping", 1);
    CHECK(Has(body, "{\"command_id\":11,\"result\":\"failed\"},{\"command_id\":12,\"result\":\"failed\"},{\"command_id\":13,\"result\":\"failed\"},{\"command_id\":14,\"result\":\"failed\"}"));
}

EWS_TEST(SyncReportNowClosesTheRunningPeriodAndSends) {
    Rig rig("sync-now");
    rig.senders.pingScript.push_back(WithCommands(FakeSenders::CommandJson(21, "report_now")));
    StartOk(rig);
    rig.Ticks(1);
    CHECK(rig.senders.batches.size() == 1);
    CHECK(Has(rig.SentLine(0, 0), "\"interval_seconds\":60"));
    rig.Ticks(1);
    CHECK(Has(rig.senders.BodyOf("/api/ping", 1), "{\"command_id\":21,\"result\":\"done\"}"));
}

EWS_TEST(SyncFlushBufferSendsTheQueue) {
    Rig rig("sync-flush");
    rig.senders.script.push_back(FakeSenders::Answer(ews::SendWord::Network, false));
    Repeat(rig.senders.pingScript, WithCommands(""), 5);
    rig.senders.pingScript.push_back(WithCommands(FakeSenders::CommandJson(31, "flush_buffer")));
    StartOk(rig);
    rig.Ticks(5);
    CHECK(!rig.Queue().empty());
    rig.Ticks(1);
    CHECK(rig.Queue().empty());
    CHECK(rig.senders.batches.size() == 2);
    rig.Ticks(1);
    CHECK(Has(rig.senders.BodyOf("/api/ping", 6), "{\"command_id\":31,\"result\":\"done\"}"));
}

EWS_TEST(SyncClearBufferEmptiesTheQueueAndLogsTheCount) {
    Rig rig("sync-clear");
    Repeat(rig.senders.script, FakeSenders::Answer(ews::SendWord::Network, false), 4);
    Repeat(rig.senders.pingScript, WithCommands(""), 10);
    rig.senders.pingScript.push_back(WithCommands(FakeSenders::CommandJson(41, "clear_buffer")));
    StartOk(rig);
    rig.Ticks(10);
    CHECK(CountLines(rig.Queue()) == 2);
    rig.Ticks(1);
    CHECK(rig.Queue().empty());
    CHECK(Has(rig.Log(), " buffer_cleared 2\n"));
    CHECK(Has(rig.Status(), "\"queued\":0"));
}

EWS_TEST(SyncPullSettingsCommandReadsTheSettingsNow) {
    Rig rig("sync-pull");
    rig.senders.pingScript.push_back(WithCommands(FakeSenders::CommandJson(51, "pull_settings")));
    rig.senders.settingsScript = {FakeSenders::Reply(200, FakeSenders::SettingsJson(1, 300, 60, 600, 3600)),
                                  FakeSenders::Reply(200, FakeSenders::SettingsJson(1, 180, 60, 600, 3600))};
    StartOk(rig);
    rig.Ticks(1);
    CHECK(rig.senders.CallsTo("/api/settings").size() == 2);
    CHECK(Has(rig.Status(), "\"interval_seconds\":180"));
    rig.Ticks(1);
    CHECK(Has(rig.senders.BodyOf("/api/ping", 1), "{\"command_id\":51,\"result\":\"done\"}"));
}

EWS_TEST(SyncUnknownCommandIsUnsupported) {
    Rig rig("sync-unknown");
    rig.senders.pingScript.push_back(WithCommands(FakeSenders::CommandJson(61, "format_disk")));
    StartOk(rig);
    rig.Ticks(2);
    CHECK(Has(rig.senders.BodyOf("/api/ping", 1), "{\"command_id\":61,\"result\":\"unsupported\"}"));
    CHECK(Has(rig.Log(), " command_run 61 0 2\n"));
}

EWS_TEST(SyncExpiredCommandIsSkippedAndNotAcked) {
    Rig rig("sync-expired");
    rig.senders.pingScript.push_back(WithCommands(FakeSenders::CommandJson(71, "report_now", "{}", "2020-01-01T00:00:00Z")));
    StartOk(rig);
    rig.Ticks(2);
    CHECK(rig.senders.batches.empty());
    CHECK(!Has(rig.senders.BodyOf("/api/ping", 1), "acks"));
}

EWS_TEST(SyncACommandThatComesAgainIsNotRunTwice) {
    Rig rig("sync-twice");
    rig.senders.pingScript.push_back(WithCommands(FakeSenders::CommandJson(7, "report_now")));
    rig.senders.pingScript.push_back(WithCommands(FakeSenders::CommandJson(7, "report_now")));
    StartOk(rig);
    rig.Ticks(2);
    CHECK(rig.senders.batches.size() == 1);
    CHECK(Has(rig.senders.BodyOf("/api/ping", 1), "{\"command_id\":7,\"result\":\"done\"}"));
    rig.Ticks(1);
    CHECK(Has(rig.senders.BodyOf("/api/ping", 2), "{\"command_id\":7,\"result\":\"done\"}"));
    rig.Ticks(1);
    CHECK(!Has(rig.senders.BodyOf("/api/ping", 3), "acks"));
    CHECK(rig.senders.batches.size() == 1);
}

EWS_TEST(SyncAcksAreDroppedOnlyAfterAPingThatGot200) {
    Rig rig("sync-ackdrop");
    rig.senders.pingScript.push_back(WithCommands(FakeSenders::CommandJson(8, "format_disk")));
    rig.senders.pingScript.push_back(FakeSenders::Reply(503, "{}"));
    StartOk(rig);
    rig.Ticks(4);
    CHECK(Has(rig.senders.BodyOf("/api/ping", 1), "{\"command_id\":8,"));
    CHECK(Has(rig.senders.BodyOf("/api/ping", 2), "{\"command_id\":8,"));
    CHECK(!Has(rig.senders.BodyOf("/api/ping", 3), "acks"));
}

EWS_TEST(SyncHandledCommandsSurviveARestartWithoutRunningAgain) {
    Rig rig("sync-donefile");
    rig.senders.pingScript.push_back(WithCommands(FakeSenders::CommandJson(9, "report_now")));
    StartOk(rig);
    rig.Ticks(1);
    CHECK(Has(rig.data.Text("status\\commands-done.json"), "{\"id\":9,\"result\":\"done\"}"));
    rig.core.Stop();
    std::size_t before = rig.senders.batches.size();
    rig.senders.pingScript.push_back(WithCommands(FakeSenders::CommandJson(9, "report_now")));
    ews::ServiceCore second(rig.deps, ews::ServiceSettings{rig.data.wide(), 60});
    CHECK(second.Start() == ews::StartResult::Ok);
    rig.clock.Advance(60);
    second.Tick();
    CHECK(rig.senders.batches.size() == before);
    second.Stop();
}

EWS_TEST(SyncUnregisteredComputerIsRegisteredOnceAnHour) {
    Rig rig("sync-register");
    ews_test::WriteText(rig.data.path() / "Client" / "device-name.txt", "  Lab PC  \nsecond line\n");
    rig.senders.pingDefault = FakeSenders::Reply(200, FakeSenders::PingJson(1, false, ""));
    StartOk(rig);
    rig.Ticks(5);
    std::vector<FakeSenders::ApiCall> puts = rig.senders.CallsTo("/api/devices/" + DeviceId(rig));
    CHECK(puts.size() == 1);
    CHECK(puts.empty() || puts[0].method == "PUT");
    CHECK(puts.empty() || puts[0].body == "{\"device_name\":\"Lab PC\",\"os\":\"windows\",\"device_type\":\"laptop\"}");
    rig.Ticks(55);
    CHECK(rig.senders.CallsTo("/api/devices/" + DeviceId(rig)).size() == 1);
    rig.Ticks(2);
    CHECK(rig.senders.CallsTo("/api/devices/" + DeviceId(rig)).size() == 2);
}

EWS_TEST(SyncRegisteredComputerIsNotRegisteredAgain) {
    Rig rig("sync-registered");
    StartOk(rig);
    rig.Ticks(3);
    CHECK(rig.senders.CallsTo("/api/devices/" + DeviceId(rig)).empty());
}

EWS_TEST(SyncRegistrationWithoutANameSendsNull) {
    Rig rig("sync-noname");
    rig.senders.pingDefault = FakeSenders::Reply(200, FakeSenders::PingJson(1, false, ""));
    StartOk(rig);
    rig.Ticks(1);
    CHECK(Has(rig.senders.BodyOf("/api/devices/" + DeviceId(rig), 0), "{\"device_name\":null,\"os\":\"windows\""));
}

EWS_TEST(SyncStatusHasTheNewFields) {
    Rig rig("sync-status");
    StartOk(rig);
    CHECK(Has(rig.Status(), "\"last_ping_result\":\"never\""));
    rig.Ticks(1);
    std::string status = rig.Status();
    CHECK(Has(status, "\"unused_threshold_seconds\":3600,\"settings_source\":\"server\",\"settings_version\":1,"));
    CHECK(Has(status, "\"last_settings_utc\":\"2026-11-14T10:00:00Z\",\"last_ping_utc\":\"2026-11-14T10:01:00Z\",\"last_ping_result\":\"ok\"}"));
}

EWS_TEST(SyncLogHoldsNoAddressKeyOrServerText) {
    Rig rig("sync-log");
    rig.senders.pingScript.push_back(FakeSenders::Reply(401, "secret server text"));
    rig.senders.settingsScript.push_back(FakeSenders::Reply(500, "secret server text"));
    StartOk(rig);
    rig.Ticks(2);
    CHECK(!Has(rig.Log(), "secret"));
    CHECK(!Has(rig.Log(), "test-key"));
    CHECK(!Has(rig.Log(), "localhost"));
}
