// test_history_file.cpp: tests of the daily history file (names, append with header, clean-up at the change of day).
#include <chrono>
#include <string>

#include "ews/history_file.h"
#include "ews/utc_stamp.h"
#include "temp_folder.h"
#include "test_common.h"

namespace {

using namespace std::chrono_literals;

/// @return A UTC time on 2026-11-14 at the given hour and minute.
ews::UtcTime Day14(int hour, int minute) {
    return std::chrono::sys_days{std::chrono::year{2026} / 11 / 14} + std::chrono::hours(hour) + std::chrono::minutes(minute);
}

/// Checks that a name is, or is not, a history file name.
void CheckName(const std::wstring& name, bool expected) {
    if (ews::IsHistoryFileName(name) != expected) {
        ews_test::Check(false, "history file name rule", __FILE__, __LINE__);
    }
}

/// @return True when the file exists in the history folder of a data folder.
bool HasFile(const std::filesystem::path& data, const char* name) { return std::filesystem::exists(data / "history" / name); }

}  // namespace

EWS_TEST(UtcStampFormats) {
    CHECK(ews::FormatUtcDate(Day14(23, 59)) == "2026-11-14");
    CHECK(ews::FormatUtcTimestamp(Day14(10, 5)) == "2026-11-14T10:05:00Z");
    CHECK(ews::FormatUtcTimestamp(Day14(0, 0) + 59s + 999ms) == "2026-11-14T00:00:59Z");
    CHECK(ews::FormatUtcDate(Day14(24, 0)) == "2026-11-15");
}

EWS_TEST(HistoryFileNames) {
    CHECK(ews::HistoryFileName(Day14(10, 0)) == L"2026-11-14.csv");
    CheckName(L"2026-11-14.csv", true);
    CheckName(L"2024-02-29.csv", true);
    CheckName(L"2026-11-14.csv.bak", false);
    CheckName(L"x2026-11-14.csv", false);
    CheckName(L"2026-11-14.CSV", false);
    CheckName(L"2026-13-14.csv", false);
    CheckName(L"2026-11-31.csv", false);
    CheckName(L"2025-02-29.csv", false);
    CheckName(L"2026-1-14.csv", false);
    CheckName(L"notes.csv", false);
    CheckName(L"", false);
    CheckName(L"2026-11-14", false);
}

EWS_TEST(HistoryNewFileGetsTheHeaderOnce) {
    auto data = ews_test::NewDataFolder("history-header");
    CHECK(ews::AppendHistoryRow(data.wstring(), Day14(10, 0), "v1,ts_utc", "1,a").ok());
    CHECK(ews::AppendHistoryRow(data.wstring(), Day14(10, 1), "v1,ts_utc", "2,b").ok());
    CHECK(ews_test::ReadText(data / "history" / "2026-11-14.csv") == "v1,ts_utc\n1,a\n2,b\n");
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(HistoryRowsGoToTheFileOfTheirUtcDay) {
    auto data = ews_test::NewDataFolder("history-days");
    CHECK(ews::AppendHistoryRow(data.wstring(), Day14(23, 59), "h", "late").ok());
    CHECK(ews::AppendHistoryRow(data.wstring(), Day14(24, 0), "h", "early").ok());
    CHECK(ews_test::ReadText(data / "history" / "2026-11-14.csv") == "h\nlate\n");
    CHECK(ews_test::ReadText(data / "history" / "2026-11-15.csv") == "h\nearly\n");
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(HistoryEmptyExistingFileGetsTheHeader) {
    auto data = ews_test::NewDataFolder("history-empty");
    ews_test::WriteText(data / "history" / "2026-11-14.csv", "");
    CHECK(ews::AppendHistoryRow(data.wstring(), Day14(10, 0), "h", "r").ok());
    CHECK(ews_test::ReadText(data / "history" / "2026-11-14.csv") == "h\nr\n");
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(HistoryFileCanBeReadWhileItIsAppended) {
    auto data = ews_test::NewDataFolder("history-share");
    CHECK(ews::AppendHistoryRow(data.wstring(), Day14(10, 0), "h", "one").ok());
    auto path = data / "history" / "2026-11-14.csv";
    HANDLE reader = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    CHECK(reader != INVALID_HANDLE_VALUE);
    CHECK(ews::AppendHistoryRow(data.wstring(), Day14(10, 1), "h", "two").ok());
    CloseHandle(reader);
    CHECK(ews_test::ReadText(path) == "h\none\ntwo\n");
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(HistoryRefusesLineBreaksAndMissingDataFolder) {
    auto data = ews_test::NewDataFolder("history-refuse");
    CHECK(!ews::AppendHistoryRow(data.wstring(), Day14(10, 0), "h", "a\nb").ok());
    CHECK(!ews::AppendHistoryRow(data.wstring(), Day14(10, 0), "h\r", "a").ok());
    CHECK(!ews::AppendHistoryRow(data.wstring(), Day14(10, 0), "h", "").ok());
    CHECK(!HasFile(data, "2026-11-14.csv"));
    CHECK(!ews::AppendHistoryRow((data / "missing").wstring(), Day14(10, 0), "h", "r").ok());
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(HistoryDeletesOnlyOldDateFiles) {
    auto data = ews_test::NewDataFolder("history-clean");
    const char* names[] = {"2026-11-09.csv", "2026-11-10.csv", "2026-11-11.csv", "2026-11-12.csv", "2026-11-13.csv",
                           "2026-11-14.csv", "2026-11-15.csv", "notes.csv", "2026-11-10.csv.bak", "2026-13-40.csv",
                           "2026-11-09.txt", "x2026-11-10.csv"};
    for (const char* name : names) {
        ews_test::WriteText(data / "history" / name, "x");
    }
    std::filesystem::create_directory(data / "history" / "2026-11-01.csv");
    auto deleted = ews::DeleteOldHistoryFiles(data.wstring(), Day14(0, 5));
    CHECK(deleted.ok());
    CHECK(deleted.value == 3);
    CHECK(!HasFile(data, "2026-11-09.csv"));
    CHECK(!HasFile(data, "2026-11-10.csv"));
    CHECK(!HasFile(data, "2026-11-11.csv"));
    CHECK(HasFile(data, "2026-11-12.csv"));
    CHECK(HasFile(data, "2026-11-13.csv"));
    CHECK(HasFile(data, "2026-11-14.csv"));
    CHECK(HasFile(data, "2026-11-15.csv"));
    CHECK(HasFile(data, "notes.csv"));
    CHECK(HasFile(data, "2026-11-10.csv.bak"));
    CHECK(HasFile(data, "2026-13-40.csv"));
    CHECK(HasFile(data, "2026-11-09.txt"));
    CHECK(HasFile(data, "x2026-11-10.csv"));
    CHECK(std::filesystem::is_directory(data / "history" / "2026-11-01.csv"));
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(HistoryCleanUpWithoutAFolderDeletesNothing) {
    auto data = ews_test::NewDataFolder("history-nofolder");
    auto result = ews::DeleteOldHistoryFiles(data.wstring(), Day14(0, 5));
    CHECK(result.ok());
    CHECK(result.value == 0);
    ews_test::RemoveDataFolder(data);
}
