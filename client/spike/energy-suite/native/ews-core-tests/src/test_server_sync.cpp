// test_server_sync.cpp: tests of the pure parts of the ping, the server settings and the commands
// (ews/server_sync.h and ews/server_api.h). No network and no file is used.
#include <string>
#include <vector>

#include "ews/server_api.h"
#include "ews/server_sync.h"
#include "service_rig.h"
#include "test_common.h"

using ews_test::FakeSenders;

namespace {

/// @return The text of a server settings answer with one value changed by a text replacement.
std::string Changed(const std::string& from, const std::string& to) {
    std::string text = FakeSenders::SettingsJson(4, 300, 60, 600, 3600);
    std::size_t at = text.find(from);
    return at == std::string::npos ? text : text.replace(at, from.size(), to);
}

/// @return True when the text is not accepted as settings of a server answer.
bool Refused(const std::string& text) { return !ews::ParseServerSettings(text, true).ok(); }

}  // namespace

EWS_TEST(SyncReportIntervalRangeAndRounding) {
    CHECK(!ews::NormaliseReportInterval(59).has_value());
    CHECK(ews::NormaliseReportInterval(60) == 60);
    CHECK(ews::NormaliseReportInterval(89) == 60);
    CHECK(ews::NormaliseReportInterval(90) == 120);
    CHECK(ews::NormaliseReportInterval(150) == 180);
    CHECK(ews::NormaliseReportInterval(3600) == 3600);
    CHECK(!ews::NormaliseReportInterval(3601).has_value());
    CHECK(!ews::NormaliseReportInterval(0).has_value());
    CHECK(!ews::NormaliseReportInterval(-300).has_value());
}

EWS_TEST(SyncPingIntervalBelow60CountsAs60AndRounds) {
    CHECK(!ews::NormalisePingInterval(0).has_value());
    CHECK(ews::NormalisePingInterval(1) == 60);
    CHECK(ews::NormalisePingInterval(59) == 60);
    CHECK(ews::NormalisePingInterval(89) == 60);
    CHECK(ews::NormalisePingInterval(90) == 120);
    CHECK(ews::NormalisePingInterval(86400) == 86400);
    CHECK(!ews::NormalisePingInterval(86401).has_value());
}

EWS_TEST(SyncSettingsOfTheContractExampleAreRead) {
    std::string text =
        R"({"settings_version":1,"report_interval_seconds":300,"ping_interval_seconds":60,"idle_threshold_seconds":600,)"
        R"("unused_threshold_seconds":3600,"no_data_after_cycles":3,"tariff_valid_from":"2026-09-14T00:00:00Z","price_gbp_per_kwh":0.25,"co2_kg_per_kwh":0.207})";
    ews::Reading<ews::ServerSettings> read = ews::ParseServerSettings(text, true);
    CHECK(read.ok());
    CHECK(read.value.version == 1);
    CHECK(read.value.reportIntervalSeconds == 300);
    CHECK(read.value.pingIntervalSeconds == 60);
    CHECK(read.value.idleThresholdSeconds == 600);
    CHECK(read.value.unusedThresholdSeconds == 3600);
}

EWS_TEST(SyncSettingsWithABadOrMissingValueAreRefusedWhole) {
    CHECK(Refused(Changed("\"report_interval_seconds\":300", "\"report_interval_seconds\":30")));
    CHECK(Refused(Changed("\"report_interval_seconds\":300", "\"report_interval_seconds\":3601")));
    CHECK(Refused(Changed("\"report_interval_seconds\":300", "\"report_interval_seconds\":\"300\"")));
    CHECK(Refused(Changed("\"report_interval_seconds\":300", "\"report_interval_seconds\":300.5")));
    CHECK(Refused(Changed("\"ping_interval_seconds\":60", "\"ping_interval_seconds\":0")));
    CHECK(Refused(Changed("\"ping_interval_seconds\":60", "\"ping_interval_seconds\":-5")));
    CHECK(Refused(Changed("\"idle_threshold_seconds\":600", "\"idle_threshold_seconds\":0")));
    CHECK(Refused(Changed("\"unused_threshold_seconds\":3600", "\"unused_threshold_seconds\":600")));
    CHECK(Refused(Changed("\"unused_threshold_seconds\":3600", "\"unused_threshold_seconds\":599")));
    CHECK(Refused(Changed("\"settings_version\":4", "\"settings_version\":-1")));
    CHECK(Refused(Changed("\"settings_version\":4,", "")));
    CHECK(Refused(Changed(",\"idle_threshold_seconds\":600", "")));
    CHECK(Refused(Changed(",\"ping_interval_seconds\":60", "")));
    CHECK(Refused("not json"));
    CHECK(Refused("[1,2]"));
    CHECK(Refused(""));
}

EWS_TEST(SyncSettingsAreRoundedToWholeMinutes) {
    ews::Reading<ews::ServerSettings> read = ews::ParseServerSettings(
        Changed("\"report_interval_seconds\":300,\"ping_interval_seconds\":60", "\"report_interval_seconds\":150,\"ping_interval_seconds\":30"), true);
    CHECK(read.ok());
    CHECK(read.value.reportIntervalSeconds == 180);
    CHECK(read.value.pingIntervalSeconds == 60);
}

EWS_TEST(SyncSettingsFileMayLackTheVersionAndKeepsTheTime) {
    std::string noVersion = Changed("\"settings_version\":4,", "");
    CHECK(!ews::ParseServerSettings(noVersion, true).ok());
    ews::Reading<ews::ServerSettings> read = ews::ParseServerSettings(noVersion, false);
    CHECK(read.ok());
    CHECK(!read.value.version.has_value());
    ews::ServerSettings settings;
    settings.version = 9;
    settings.reportIntervalSeconds = 600;
    settings.checkedUnix = 1794650700;
    std::string text = ews::FormatServerSettings(settings);
    CHECK(text.back() == '\n');
    ews::Reading<ews::ServerSettings> back = ews::ParseServerSettings(text, true);
    CHECK(back.ok());
    CHECK(back.value.version == 9);
    CHECK(back.value.reportIntervalSeconds == 600);
    CHECK(back.value.checkedUnix == 1794650700);
}

EWS_TEST(SyncPingAnswerOfTheContractExampleIsRead) {
    std::string text = R"({"settings_version": 8, "registered": true, "commands": [{"id": 42, "type": "report_now", "params": {}, "expires_at": "2026-10-19T14:30:00Z"}]})";
    ews::Reading<ews::PingAnswer> read = ews::ParsePingAnswer(text);
    CHECK(read.ok());
    CHECK(read.value.settingsVersion == 8);
    CHECK(read.value.registered);
    CHECK(read.value.commands.size() == 1);
    CHECK(read.value.commands.empty() || read.value.commands[0].id == 42);
    CHECK(read.value.commands.empty() || read.value.commands[0].type == "report_now");
    CHECK(read.value.commands.empty() || read.value.commands[0].expiresUnix == 1792420200);
}

EWS_TEST(SyncPingAnswerCommandsComeInIdOrderAndBadOnesAreLeftOut) {
    std::string commands = FakeSenders::CommandJson(9, "flush_buffer") + "," + FakeSenders::CommandJson(3, "set_interval", R"({"report_interval_seconds":600})") +
                           R"(,{"id":5,"type":"report_now","params":{}})" + R"(,{"id":0,"type":"x","params":{},"expires_at":"2099-01-01T00:00:00Z"})" +
                           R"(,{"id":6,"type":"","params":{},"expires_at":"2099-01-01T00:00:00Z"})";
    ews::Reading<ews::PingAnswer> read = ews::ParsePingAnswer(FakeSenders::PingJson(2, false, commands));
    CHECK(read.ok());
    CHECK(!read.value.registered);
    CHECK(read.value.commands.size() == 2);
    CHECK(read.value.commands.size() < 2 || read.value.commands[0].id == 3);
    CHECK(read.value.commands.size() < 2 || read.value.commands[0].intervalParam == 600);
    CHECK(read.value.commands.size() < 2 || read.value.commands[1].id == 9);
    CHECK(read.value.commands.size() < 2 || !read.value.commands[1].intervalParam.has_value());
}

EWS_TEST(SyncPingAnswerWithAWrongShapeIsRefused) {
    CHECK(!ews::ParsePingAnswer("").ok());
    CHECK(!ews::ParsePingAnswer("[]").ok());
    CHECK(!ews::ParsePingAnswer(R"({"registered":true,"commands":[]})").ok());
    CHECK(!ews::ParsePingAnswer(R"({"settings_version":1,"commands":[]})").ok());
    CHECK(!ews::ParsePingAnswer(R"({"settings_version":1,"registered":true})").ok());
    CHECK(!ews::ParsePingAnswer(R"({"settings_version":1.5,"registered":true,"commands":[]})").ok());
    CHECK(!ews::ParsePingAnswer(R"({"settings_version":1,"registered":"yes","commands":[]})").ok());
    std::string many;
    for (int id = 1; id <= 51; ++id) {
        many += (many.empty() ? "" : ",") + FakeSenders::CommandJson(id, "report_now");
    }
    CHECK(!ews::ParsePingAnswer(FakeSenders::PingJson(1, true, many)).ok());
}

EWS_TEST(SyncPingBodyIsExact) {
    std::string id = "6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a12";
    CHECK(ews::BuildPingBody(id, 2, {}) == R"({"device_id":"6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a12","unsent_reports":2})");
    std::vector<ews::DoneCommand> acks{{41, ews::CommandResult::Done}, {42, ews::CommandResult::Unsupported}, {43, ews::CommandResult::Failed}};
    CHECK(ews::BuildPingBody(id, 0, acks) ==
          R"({"device_id":"6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a12","unsent_reports":0,"acks":[{"command_id":41,"result":"done"},{"command_id":42,"result":"unsupported"},{"command_id":43,"result":"failed"}]})");
}

EWS_TEST(SyncDeviceBodyAndNameFile) {
    CHECK(ews::BuildDeviceBody(std::string("Lab 1 PC 03"), "laptop") == R"({"device_name":"Lab 1 PC 03","os":"windows","device_type":"laptop"})");
    CHECK(ews::BuildDeviceBody(std::nullopt, "desktop") == R"({"device_name":null,"os":"windows","device_type":"desktop"})");
    CHECK(ews::DeviceNameFromFile("  Lab PC  \r\nsecond line\n") == "Lab PC");
    CHECK(!ews::DeviceNameFromFile("").has_value());
    CHECK(!ews::DeviceNameFromFile("   \nsecond").has_value());
    CHECK(!ews::DeviceNameFromFile("bad\x01name").has_value());
    CHECK(ews::DeviceNameFromFile(std::string(80, 'a')) == std::string(60, 'a'));
    std::string accents;
    for (int i = 0; i < 70; ++i) {
        accents += "\xC5\x82";
    }
    std::optional<std::string> cut = ews::DeviceNameFromFile(accents);
    CHECK(cut.has_value());
    CHECK(!cut.has_value() || cut->size() == 120);
}

EWS_TEST(SyncDoneFileRoundTripAndBrokenFile) {
    std::vector<ews::DoneCommand> done{{7, ews::CommandResult::Done}, {8, ews::CommandResult::Failed}, {9, ews::CommandResult::Unsupported}};
    std::vector<ews::DoneCommand> back = ews::ParseCommandsDone(ews::FormatCommandsDone(done));
    CHECK(back.size() == 3);
    CHECK(back.size() < 3 || (back[0].id == 7 && back[1].result == ews::CommandResult::Failed && back[2].result == ews::CommandResult::Unsupported));
    CHECK(ews::ParseCommandsDone("garbage").empty());
    CHECK(ews::ParseCommandsDone(R"({"done":[{"id":"x","result":"done"},{"id":5}]})").empty());
    std::vector<ews::DoneCommand> many;
    for (int id = 1; id <= 250; ++id) {
        many.push_back({id, ews::CommandResult::Done});
    }
    std::vector<ews::DoneCommand> kept = ews::ParseCommandsDone(ews::FormatCommandsDone(many));
    CHECK(kept.size() == 200);
    CHECK(kept.empty() || kept.front().id == 51);
}

EWS_TEST(SyncAckedCommandsLeaveExceptThoseListedAgain) {
    std::vector<ews::DoneCommand> done{{1, ews::CommandResult::Done}, {2, ews::CommandResult::Done}, {3, ews::CommandResult::Done}};
    std::vector<ews::DoneCommand> acked{{1, ews::CommandResult::Done}, {2, ews::CommandResult::Done}};
    ews::ServerCommand again;
    again.id = 2;
    std::vector<ews::DoneCommand> kept = ews::WithoutAcked(done, acked, {again});
    CHECK(kept.size() == 2);
    CHECK(!ews::HasDone(kept, 1));
    CHECK(ews::HasDone(kept, 2));
    CHECK(ews::HasDone(kept, 3));
}

EWS_TEST(SyncAnswerWordsAndRetryAfter) {
    CHECK(ews::ApiWordFor(200) == ews::ApiWord::Ok);
    CHECK(ews::ApiWordFor(401) == ews::ApiWord::Unauthorised);
    CHECK(ews::ApiWordFor(429) == ews::ApiWord::RateLimited);
    CHECK(ews::ApiWordFor(503) == ews::ApiWord::ServerError);
    CHECK(ews::ApiWordFor(500) == ews::ApiWord::ServerError);
    CHECK(ews::ApiWordFor(302) == ews::ApiWord::ServerError);
    CHECK(ews::ApiWordFor(201) == ews::ApiWord::ServerError);
    CHECK(ews::ApiWordFor(422) == ews::ApiWord::Rejected);
    CHECK(ews::ApiWordFor(404) == ews::ApiWord::Rejected);
    CHECK(ews::ApiWordFor(0) == ews::ApiWord::Network);
    CHECK(std::string(ews::ApiWordText(ews::ApiWord::RateLimited)) == "rate_limited");
    CHECK(ews::ParseRetryAfter("120") == 120);
    CHECK(ews::ParseRetryAfter("") == 0);
    CHECK(ews::ParseRetryAfter("Wed, 21 Oct 2026 07:28:00 GMT") == 0);
    CHECK(ews::ParseRetryAfter("-5") == 0);
    CHECK(ews::ParseRetryAfter("12345678901") == 0);
    CHECK(ews::HoldSecondsFor(FakeSenders::Reply(429, "", 90)) == 90);
    CHECK(ews::HoldSecondsFor(FakeSenders::Reply(503, "", 7200)) == 3600);
    CHECK(ews::HoldSecondsFor(FakeSenders::Reply(401, "", 90)) == 0);
    CHECK(ews::HoldSecondsFor(FakeSenders::Reply(429, "")) == 0);
}
