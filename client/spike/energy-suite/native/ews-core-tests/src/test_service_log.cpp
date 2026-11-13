// test_service_log.cpp: tests of the service log (line format, fixed words, roll-over, two writers at once).
#include <chrono>
#include <string>
#include <thread>

#include "ews/service_log.h"
#include "temp_folder.h"
#include "test_common.h"

namespace {

/// @return A UTC time on 2026-11-14 at 10:05:00.
ews::UtcTime Noon() {
    return std::chrono::sys_days{std::chrono::year{2026} / 11 / 14} + std::chrono::hours(10) + std::chrono::minutes(5);
}

/// Appends `count` lines of one event; used by the two writer threads.
void WriteMany(const std::wstring& data, ews::LogEvent event, int count) {
    for (int number = 0; number < count; ++number) {
        ews::AppendLog(data, Noon(), event, {number});
    }
}

/// @return How many lines of a text start with a given prefix.
int CountLinesWith(const std::string& text, const std::string& prefix) {
    int count = 0;
    std::size_t position = text.find(prefix);
    while (position != std::string::npos) {
        ++count;
        position = text.find(prefix, position + 1);
    }
    return count;
}

}  // namespace

EWS_TEST(LogEventWordsAreFixed) {
    CHECK(std::string(ews::LogEventWord(ews::LogEvent::ServiceStart)) == "service_start");
    CHECK(std::string(ews::LogEventWord(ews::LogEvent::ServiceStop)) == "service_stop");
    CHECK(std::string(ews::LogEventWord(ews::LogEvent::LevelChange)) == "level_change");
    CHECK(std::string(ews::LogEventWord(ews::LogEvent::SendOk)) == "send_ok");
    CHECK(std::string(ews::LogEventWord(ews::LogEvent::SendUnauthorised)) == "send_unauthorised");
    CHECK(std::string(ews::LogEventWord(ews::LogEvent::SendRejected)) == "send_rejected");
    CHECK(std::string(ews::LogEventWord(ews::LogEvent::SendServerError)) == "send_server_error");
    CHECK(std::string(ews::LogEventWord(ews::LogEvent::SendNetwork)) == "send_network");
    CHECK(std::string(ews::LogEventWord(ews::LogEvent::SendDisabled)) == "send_disabled");
    CHECK(std::string(ews::LogEventWord(ews::LogEvent::ConfigBadValues)) == "config_bad_values");
    CHECK(std::string(ews::LogEventWord(ews::LogEvent::QueueDropped)) == "queue_dropped");
    CHECK(std::string(ews::LogEventWord(ews::LogEvent::QueueSkipped)) == "queue_skipped");
    CHECK(std::string(ews::LogEventWord(ews::LogEvent::QueueBrokenFile)) == "queue_broken_file");
    CHECK(std::string(ews::LogEventWord(ews::LogEvent::QueueWriteFailed)) == "queue_write_failed");
    CHECK(std::string(ews::LogEventWord(ews::LogEvent::HistoryWriteFailed)) == "history_write_failed");
    CHECK(std::string(ews::LogEventWord(ews::LogEvent::HistoryCleaned)) == "history_cleaned");
    CHECK(std::string(ews::LogEventWord(ews::LogEvent::CountedError)) == "counted_error");
}

EWS_TEST(LogLineFormat) {
    CHECK(ews::FormatLogLine(Noon(), ews::LogEvent::ServiceStart, {}) == "2026-11-14T10:05:00Z service_start\n");
    CHECK(ews::FormatLogLine(Noon(), ews::LogEvent::SendOk, {5}) == "2026-11-14T10:05:00Z send_ok 5\n");
    CHECK(ews::FormatLogLine(Noon(), ews::LogEvent::SendOk, {5, -2}) == "2026-11-14T10:05:00Z send_ok 5 -2\n");
    CHECK(ews::FormatLogLine(Noon(), ews::LogEvent::SendOk, {1, 2, 3}) == "2026-11-14T10:05:00Z send_ok 1 2 3\n");
    CHECK(ews::FormatLogLine(Noon(), ews::LogEvent::SendOk, {1, 2, 3, 4, 5}) == "2026-11-14T10:05:00Z send_ok 1 2 3\n");
}

EWS_TEST(LogAppendsLines) {
    auto data = ews_test::NewDataFolder("log-append");
    CHECK(ews::AppendLog(data.wstring(), Noon(), ews::LogEvent::ServiceStart).ok());
    CHECK(ews::AppendLog(data.wstring(), Noon(), ews::LogEvent::SendOk, {3, 1}).ok());
    CHECK(ews_test::ReadText(data / "logs" / "service.log") == "2026-11-14T10:05:00Z service_start\n2026-11-14T10:05:00Z send_ok 3 1\n");
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(LogRollsOverAt1Mb) {
    auto data = ews_test::NewDataFolder("log-roll");
    const std::string filler(ews::kMaxLogBytes, 'x');
    ews_test::WriteText(data / "logs" / "service.log", filler);
    ews_test::WriteText(data / "logs" / "service.previous.log", "older");
    CHECK(ews::AppendLog(data.wstring(), Noon(), ews::LogEvent::ServiceStop).ok());
    CHECK(ews_test::ReadText(data / "logs" / "service.previous.log") == filler);
    CHECK(ews_test::ReadText(data / "logs" / "service.log") == "2026-11-14T10:05:00Z service_stop\n");
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(LogDoesNotRollOverBelow1Mb) {
    auto data = ews_test::NewDataFolder("log-noroll");
    ews_test::WriteText(data / "logs" / "service.log", std::string(ews::kMaxLogBytes - 1, 'x'));
    CHECK(ews::AppendLog(data.wstring(), Noon(), ews::LogEvent::ServiceStop).ok());
    CHECK(!std::filesystem::exists(data / "logs" / "service.previous.log"));
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(LogFailsForAMissingDataFolder) {
    auto data = ews_test::NewDataFolder("log-nodata");
    CHECK(!ews::AppendLog((data / "missing").wstring(), Noon(), ews::LogEvent::ServiceStart).ok());
    CHECK(!std::filesystem::exists(data / "missing"));
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(LogTwoWritersNeverMixALine) {
    auto data = ews_test::NewDataFolder("log-two");
    std::wstring folder = data.wstring();
    CHECK(ews::AppendLog(folder, Noon(), ews::LogEvent::ServiceStart).ok());
    std::thread first(WriteMany, folder, ews::LogEvent::SendOk, 100);
    std::thread second(WriteMany, folder, ews::LogEvent::SendNetwork, 100);
    first.join();
    second.join();
    std::string text = ews_test::ReadText(data / "logs" / "service.log");
    CHECK(CountLinesWith(text, "\n2026-11-14T10:05:00Z send_ok ") == 100);
    CHECK(CountLinesWith(text, "\n2026-11-14T10:05:00Z send_network ") == 100);
    CHECK(CountLinesWith(text, "\n") == 201);
    ews_test::RemoveDataFolder(data);
}
