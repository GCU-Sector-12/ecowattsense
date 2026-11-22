// test_configure.cpp: tests of the `--configure` input (url= and key= lines).
#include <string>

#include "ews/config.h"
#include "ews/configure.h"
#include "ews/secret.h"
#include "temp_folder.h"
#include "test_common.h"

namespace {

/// @return The fixed word of the result of one input, for a short comparison.
std::string Apply(const std::string& input, const std::filesystem::path& data) {
    return ews::ConfigureResultWord(ews::ApplyConfigureInput(input, data.wstring()));
}

}  // namespace

EWS_TEST(ConfigureResultWords) {
    CHECK(std::string(ews::ConfigureResultWord(ews::ConfigureResult::Ok)) == "ok");
    CHECK(std::string(ews::ConfigureResultWord(ews::ConfigureResult::BadUrl)) == "bad_url");
    CHECK(std::string(ews::ConfigureResultWord(ews::ConfigureResult::BadKey)) == "bad_key");
    CHECK(std::string(ews::ConfigureResultWord(ews::ConfigureResult::TooLong)) == "too_long");
    CHECK(std::string(ews::ConfigureResultWord(ews::ConfigureResult::WriteFailed)) == "write_failed");
}

EWS_TEST(ConfigureWritesUrlAndKeyAndKeepsOtherKeys) {
    auto data = ews_test::NewDataFolder("configure-both");
    ews_test::WriteText(data / "config" / "service.ini", "interval_seconds=120\n");
    CHECK(Apply("url=https://example.org:8443\nkey=abc-123\n", data) == "ok");
    CHECK(ews_test::ReadText(data / "config" / "service.ini") == "interval_seconds=120\nserver_url=https://example.org:8443\n");
    CHECK(ews::LoadServerKey(data.wstring()).value == "abc-123");
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(ConfigureAcceptsCrLfAndNoFinalLineEnd) {
    auto data = ews_test::NewDataFolder("configure-crlf");
    CHECK(Apply("url=http://localhost:8080\r\nkey=k1", data) == "ok");
    CHECK(ews::LoadServiceConfig(data.wstring()).value.serverUrl == "http://localhost:8080");
    CHECK(ews::LoadServerKey(data.wstring()).value == "k1");
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(ConfigureOnlyUrlKeepsTheStoredKey) {
    auto data = ews_test::NewDataFolder("configure-url");
    CHECK(Apply("url=https://one.org\nkey=first-key\n", data) == "ok");
    CHECK(Apply("url=https://two.org\n", data) == "ok");
    CHECK(ews::LoadServiceConfig(data.wstring()).value.serverUrl == "https://two.org");
    CHECK(ews::LoadServerKey(data.wstring()).value == "first-key");
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(ConfigureOnlyKeyKeepsTheStoredUrl) {
    auto data = ews_test::NewDataFolder("configure-key");
    CHECK(Apply("url=https://one.org\nkey=first-key\n", data) == "ok");
    CHECK(Apply("key=second-key\n", data) == "ok");
    CHECK(ews::LoadServiceConfig(data.wstring()).value.serverUrl == "https://one.org");
    CHECK(ews::LoadServerKey(data.wstring()).value == "second-key");
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(ConfigureBadUrlWritesNothing) {
    auto data = ews_test::NewDataFolder("configure-badurl");
    CHECK(Apply("url=http://example.org\nkey=good-key\n", data) == "bad_url");
    CHECK(Apply("url=\nkey=good-key\n", data) == "bad_url");
    CHECK(Apply("url=https://example.org/path\n", data) == "bad_url");
    CHECK(!std::filesystem::exists(data / "config" / "service.ini"));
    CHECK(!std::filesystem::exists(data / "config" / "key.bin"));
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(ConfigureBadKeyWritesNothing) {
    auto data = ews_test::NewDataFolder("configure-badkey");
    CHECK(Apply("url=https://example.org\nkey=\n", data) == "bad_key");
    CHECK(Apply("url=https://example.org\nkey= leading\n", data) == "bad_key");
    CHECK(Apply("url=https://example.org\nkey=trailing \n", data) == "bad_key");
    CHECK(Apply("url=https://example.org\nkey=ab\rcd\n", data) == "bad_key");
    CHECK(Apply(std::string("url=https://example.org\nkey=ab\0cd\n", 34), data) == "bad_key");
    CHECK(Apply("url=https://example.org\nkey=\xc3\xa9\n", data) == "bad_key");
    CHECK(!std::filesystem::exists(data / "config" / "service.ini"));
    CHECK(!std::filesystem::exists(data / "config" / "key.bin"));
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(ConfigureKeyWithALineBreakEndsAtTheBreak) {
    auto data = ews_test::NewDataFolder("configure-linebreak");
    // The text after the line break is a new line that is neither url= nor key=; it is ignored and never stored.
    CHECK(Apply("key=first\nsecond-part\n", data) == "ok");
    CHECK(ews::LoadServerKey(data.wstring()).value == "first");
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(ConfigureRefusesInputOver4Kb) {
    auto data = ews_test::NewDataFolder("configure-long");
    std::string longInput = "url=https://example.org\nkey=" + std::string(4 * 1024, 'k') + "\n";
    CHECK(Apply(longInput, data) == "too_long");
    CHECK(Apply("url=https://example.org\n" + std::string(4 * 1024, '\n'), data) == "too_long");
    CHECK(!std::filesystem::exists(data / "config" / "service.ini"));
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(ConfigureInputWithNeitherLineIsRefused) {
    auto data = ews_test::NewDataFolder("configure-none");
    CHECK(Apply("", data) == "bad_url");
    CHECK(Apply("hello\nworld\n", data) == "bad_url");
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(ConfigureLastLineWins) {
    auto data = ews_test::NewDataFolder("configure-last");
    CHECK(Apply("url=https://one.org\nurl=https://two.org\nkey=a\nkey=b\n", data) == "ok");
    CHECK(ews::LoadServiceConfig(data.wstring()).value.serverUrl == "https://two.org");
    CHECK(ews::LoadServerKey(data.wstring()).value == "b");
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(ConfigureMissingDataFolderIsWriteFailed) {
    auto data = ews_test::NewDataFolder("configure-nodata");
    CHECK(Apply("url=https://example.org\nkey=abc\n", data / "missing") == "write_failed");
    ews_test::RemoveDataFolder(data);
}
