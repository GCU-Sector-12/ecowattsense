// test_config.cpp: tests of service.ini parsing, the URL and device type rules, and the ini writer.
#include <string>

#include "ews/config.h"
#include "temp_folder.h"
#include "test_common.h"

namespace {

/// Checks that IsValidServerUrl gives the expected answer for one URL.
void CheckUrl(const std::string& url, bool expected) {
    if (ews::IsValidServerUrl(url) != expected) {
        ews_test::Check(false, ("url rule for: " + url).c_str(), __FILE__, __LINE__);
    }
}

/// Checks that IsValidDeviceType gives the expected answer for one text.
void CheckType(const std::string& type, bool expected) {
    if (ews::IsValidDeviceType(type) != expected) {
        ews_test::Check(false, ("device type rule for: " + type).c_str(), __FILE__, __LINE__);
    }
}

}  // namespace

EWS_TEST(ConfigAcceptsGoodUrls) {
    CheckUrl("https://example.org", true);
    CheckUrl("https://example.org:8443", true);
    CheckUrl("https://sub.example-host.org", true);
    CheckUrl("http://localhost:8080", true);
    CheckUrl("http://localhost", true);
    CheckUrl("http://127.0.0.1:8080", true);
    CheckUrl("http://[::1]:8080", true);
    CheckUrl("https://localhost", true);
}

EWS_TEST(ConfigRefusesBadUrls) {
    CheckUrl("", false);
    CheckUrl("http://example.org", false);
    CheckUrl("https://example.org/path", false);
    CheckUrl("https://example.org/", false);
    CheckUrl("https://user@example.org", false);
    CheckUrl("https://user:pw@example.org", false);
    CheckUrl("ftp://example.org", false);
    CheckUrl("https://", false);
    CheckUrl("https://example.org:", false);
    CheckUrl("https://example.org:0", false);
    CheckUrl("https://example.org:65536", false);
    CheckUrl("https://example.org:80a", false);
    CheckUrl("https://example.org?x=1", false);
    CheckUrl("https://example.org#top", false);
    CheckUrl("https://exa mple.org", false);
    CheckUrl("https://example.org ", false);
    CheckUrl("https://.example.org", false);
    CheckUrl("https://example..org", false);
    CheckUrl("http://localhost.evil.org", false);
    CheckUrl("http://127.0.0.2:8080", false);
    CheckUrl("http://[::2]:8080", false);
    CheckUrl("HTTPS://example.org", false);
    CheckUrl(std::string("https://example.org\r\nX: y"), false);
    CheckUrl("https://" + std::string(400, 'a') + ".org", false);
}

EWS_TEST(ConfigDeviceTypeRule) {
    CheckType("laptop", true);
    CheckType("mini_pc2", true);
    CheckType("a", true);
    CheckType(std::string(40, 'a'), true);
    CheckType(std::string(41, 'a'), false);
    CheckType("", false);
    CheckType("1laptop", false);
    CheckType("_laptop", false);
    CheckType("Laptop", false);
    CheckType("lap top", false);
    CheckType("lap-top", false);
    CheckType("laptop\n", false);
}

EWS_TEST(ConfigDefaultsForEmptyText) {
    ews::ServiceConfig config = ews::ParseServiceConfig("");
    CHECK(config.serverEnabled);
    CHECK(config.serverUrl.empty());
    CHECK(config.intervalSeconds == 300);
    CHECK(config.idleThresholdSeconds == 600);
    CHECK(config.unusedThresholdSeconds == 3600);
    CHECK(config.deviceType == "laptop");
    CHECK(config.badValues == 0);
    CHECK(!config.SendingOn());
}

EWS_TEST(ConfigReadsEveryKey) {
    const std::string text =
        "server_enabled=1\nserver_url=https://example.org:8443\ninterval_seconds=600\n"
        "idle_threshold_seconds=120\nunused_threshold_seconds=7200\ndevice_type=desktop_pc\nunknown_key=whatever\n";
    ews::ServiceConfig config = ews::ParseServiceConfig(text);
    CHECK(config.serverEnabled);
    CHECK(config.serverUrl == "https://example.org:8443");
    CHECK(config.intervalSeconds == 600);
    CHECK(config.idleThresholdSeconds == 120);
    CHECK(config.unusedThresholdSeconds == 7200);
    CHECK(config.deviceType == "desktop_pc");
    CHECK(config.badValues == 0);
    CHECK(config.SendingOn());
}

EWS_TEST(ConfigAcceptsCrLfBlanksAndLastWins) {
    ews::ServiceConfig config = ews::ParseServiceConfig("interval_seconds=120\r\n  device_type = tablet \r\ninterval_seconds=180");
    CHECK(config.intervalSeconds == 180);
    CHECK(config.deviceType == "tablet");
    CHECK(config.badValues == 0);
}

EWS_TEST(ConfigServerEnabledZeroTurnsSendingOff) {
    ews::ServiceConfig config = ews::ParseServiceConfig("server_enabled=0\nserver_url=https://example.org\n");
    CHECK(!config.serverEnabled);
    CHECK(!config.SendingOn());
    CHECK(config.badValues == 0);
}

EWS_TEST(ConfigBadValuesFallBackAndAreCounted) {
    const std::string text =
        "server_enabled=2\ninterval_seconds=90\nidle_threshold_seconds=30\ndevice_type=Bad Type\n";
    ews::ServiceConfig config = ews::ParseServiceConfig(text);
    CHECK(config.serverEnabled);
    CHECK(config.intervalSeconds == 300);
    CHECK(config.idleThresholdSeconds == 600);
    CHECK(config.deviceType == "laptop");
    CHECK(config.badValues == 4);
}

EWS_TEST(ConfigIntervalRange) {
    CHECK(ews::ParseServiceConfig("interval_seconds=60").intervalSeconds == 60);
    CHECK(ews::ParseServiceConfig("interval_seconds=3600").intervalSeconds == 3600);
    CHECK(ews::ParseServiceConfig("interval_seconds=0").intervalSeconds == 300);
    CHECK(ews::ParseServiceConfig("interval_seconds=3660").intervalSeconds == 300);
    CHECK(ews::ParseServiceConfig("interval_seconds=-60").intervalSeconds == 300);
    CHECK(ews::ParseServiceConfig("interval_seconds=99999999999999").intervalSeconds == 300);
    CHECK(ews::ParseServiceConfig("interval_seconds=abc").badValues == 1);
    CHECK(ews::ParseServiceConfig("interval_seconds=").badValues == 1);
}

EWS_TEST(ConfigThresholdRange) {
    CHECK(ews::ParseServiceConfig("idle_threshold_seconds=60").idleThresholdSeconds == 60);
    CHECK(ews::ParseServiceConfig("idle_threshold_seconds=86400\nunused_threshold_seconds=86401").idleThresholdSeconds == 86400);
    CHECK(ews::ParseServiceConfig("idle_threshold_seconds=86401").idleThresholdSeconds == 600);
    CHECK(ews::ParseServiceConfig("unused_threshold_seconds=604800").unusedThresholdSeconds == 604800);
    CHECK(ews::ParseServiceConfig("unused_threshold_seconds=604801").unusedThresholdSeconds == 3600);
}

EWS_TEST(ConfigUnusedMustBeAboveIdle) {
    ews::ServiceConfig equal = ews::ParseServiceConfig("idle_threshold_seconds=900\nunused_threshold_seconds=900");
    CHECK(equal.unusedThresholdSeconds == 3600);
    CHECK(equal.badValues == 1);
    ews::ServiceConfig below = ews::ParseServiceConfig("idle_threshold_seconds=900\nunused_threshold_seconds=600");
    CHECK(below.unusedThresholdSeconds == 3600);
    // An idle threshold that makes the default unused value too small: both go back to their defaults.
    ews::ServiceConfig both = ews::ParseServiceConfig("idle_threshold_seconds=7200");
    CHECK(both.idleThresholdSeconds == 600);
    CHECK(both.unusedThresholdSeconds == 3600);
    CHECK(both.badValues == 1);
}

EWS_TEST(ConfigBadUrlLeavesUrlEmptyAndSendingOff) {
    ews::ServiceConfig config = ews::ParseServiceConfig("server_enabled=1\nserver_url=http://example.org\n");
    CHECK(config.serverUrl.empty());
    CHECK(config.badValues == 1);
    CHECK(!config.SendingOn());
}

EWS_TEST(ConfigLoadFromFile) {
    auto data = ews_test::NewDataFolder("config-load");
    ews_test::WriteText(data / "config" / "service.ini", "server_url=https://example.org\ninterval_seconds=120\n");
    auto loaded = ews::LoadServiceConfig(data.wstring());
    CHECK(loaded.ok());
    CHECK(loaded.value.serverUrl == "https://example.org");
    CHECK(loaded.value.intervalSeconds == 120);
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(ConfigLoadMissingFileGivesDefaults) {
    auto data = ews_test::NewDataFolder("config-missing");
    auto loaded = ews::LoadServiceConfig(data.wstring());
    CHECK(loaded.status == ews::Status::Unavailable);
    CHECK(loaded.value.intervalSeconds == 300);
    CHECK(!loaded.value.SendingOn());
    auto noFolder = ews::LoadServiceConfig((data / "nothing").wstring());
    CHECK(!noFolder.ok());
    CHECK(noFolder.value.intervalSeconds == 300);
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(ConfigLoadRefusesAFileOver16Kb) {
    auto data = ews_test::NewDataFolder("config-big");
    ews_test::WriteText(data / "config" / "service.ini", std::string(16 * 1024 + 1, '#'));
    auto loaded = ews::LoadServiceConfig(data.wstring());
    CHECK(!loaded.ok());
    CHECK(loaded.value.intervalSeconds == 300);
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(ConfigSetIniValueReplacesOrAppends) {
    CHECK(ews::SetIniValue("", "server_url", "https://a.org") == "server_url=https://a.org\n");
    CHECK(ews::SetIniValue("interval_seconds=60", "server_url", "https://a.org") == "interval_seconds=60\nserver_url=https://a.org\n");
    CHECK(ews::SetIniValue("a=1\nserver_url=old\nb=2\n", "server_url", "https://a.org") == "a=1\nserver_url=https://a.org\nb=2\n");
    CHECK(ews::SetIniValue("# note\nserver_url=old\nserver_url=older\n", "server_url", "x") == "# note\nserver_url=x\nserver_url=x\n");
}

EWS_TEST(ConfigSaveServerUrlKeepsOtherKeysAndCreatesFile) {
    auto data = ews_test::NewDataFolder("config-save");
    CHECK(ews::SaveServerUrl(data.wstring(), "https://one.org").ok());
    CHECK(ews_test::ReadText(data / "config" / "service.ini") == "server_url=https://one.org\n");
    ews_test::WriteText(data / "config" / "service.ini", "interval_seconds=120\nserver_url=https://old.org\ndevice_type=tablet\n");
    CHECK(ews::SaveServerUrl(data.wstring(), "https://two.org").ok());
    CHECK(ews_test::ReadText(data / "config" / "service.ini") == "interval_seconds=120\nserver_url=https://two.org\ndevice_type=tablet\n");
    CHECK(!ews::SaveServerUrl((data / "missing").wstring(), "https://x.org").ok());
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(ConfigSaveServerUrlDoesNotOverwriteAnUnreadableFile) {
    auto data = ews_test::NewDataFolder("config-save-big");
    const std::string big(16 * 1024 + 1, '#');
    ews_test::WriteText(data / "config" / "service.ini", big);
    CHECK(!ews::SaveServerUrl(data.wstring(), "https://x.org").ok());
    CHECK(ews_test::ReadText(data / "config" / "service.ini") == big);
    ews_test::RemoveDataFolder(data);
}
