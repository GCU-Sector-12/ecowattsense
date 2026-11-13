// test_report_queue.cpp: tests of the report queue file (append, cap, broken last line, oversized file, peek, remove).
#include <string>

#include "ews/report_queue.h"
#include "temp_folder.h"
#include "test_common.h"

namespace {

/// @return The path of the queue file below a data folder.
std::filesystem::path QueueFile(const std::filesystem::path& data) { return data / "queue" / "reports.jsonl"; }

/// Appends the lines "L<from>" up to "L<to-1>" and checks that each was added.
void AppendRange(const std::filesystem::path& data, int from, int to) {
    for (int number = from; number < to; ++number) {
        auto result = ews::AppendReport(data.wstring(), "L" + std::to_string(number));
        CHECK(result.outcome == ews::QueueAppend::Added);
    }
}

/// Makes the text of `count` lines "L0" to "L<count-1>", each with a line feed.
std::string LinesText(int count) {
    std::string text;
    for (int number = 0; number < count; ++number) {
        text += "L" + std::to_string(number) + "\n";
    }
    return text;
}

}  // namespace

EWS_TEST(QueueLineRule) {
    CHECK(ews::IsQueueableLine("{}"));
    CHECK(ews::IsQueueableLine(std::string(4096, 'a')));
    CHECK(!ews::IsQueueableLine(std::string(4097, 'a')));
    CHECK(!ews::IsQueueableLine(""));
    CHECK(!ews::IsQueueableLine("a\nb"));
    CHECK(!ews::IsQueueableLine("a\rb"));
    CHECK(!ews::IsQueueableLine("a\n"));
}

EWS_TEST(QueueAppendPeekAndRemove) {
    auto data = ews_test::NewDataFolder("queue-basic");
    AppendRange(data, 0, 5);
    CHECK(ews_test::ReadText(QueueFile(data)) == LinesText(5));
    auto peek = ews::PeekReports(data.wstring(), 3);
    CHECK(peek.ok());
    CHECK(peek.value.lines.size() == 3);
    CHECK(ews_test::LineAt(peek.value.lines, 0) == "L0");
    CHECK(ews_test::LineAt(peek.value.lines, 2) == "L2");
    CHECK(ews_test::ReadText(QueueFile(data)) == LinesText(5));
    CHECK(ews::RemoveFirstReports(data.wstring(), 3).ok());
    CHECK(ews_test::ReadText(QueueFile(data)) == "L3\nL4\n");
    CHECK(ews::RemoveFirstReports(data.wstring(), 50).ok());
    CHECK(ews::PeekReports(data.wstring(), 10).value.lines.empty());
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(QueueMissingFileIsEmpty) {
    auto data = ews_test::NewDataFolder("queue-missing");
    auto peek = ews::PeekReports(data.wstring(), 10);
    CHECK(peek.ok());
    CHECK(peek.value.lines.empty());
    CHECK(ews::RemoveFirstReports(data.wstring(), 1).ok());
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(QueueRefusesBadLinesAndChangesNothing) {
    auto data = ews_test::NewDataFolder("queue-refuse");
    AppendRange(data, 0, 1);
    CHECK(ews::AppendReport(data.wstring(), "").outcome == ews::QueueAppend::Refused);
    CHECK(ews::AppendReport(data.wstring(), "a\nb").outcome == ews::QueueAppend::Refused);
    CHECK(ews::AppendReport(data.wstring(), std::string(4097, 'x')).outcome == ews::QueueAppend::Refused);
    CHECK(ews_test::ReadText(QueueFile(data)) == "L0\n");
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(QueuePeekReturnsAtMost100AndClampsTheRequest) {
    auto data = ews_test::NewDataFolder("queue-100");
    ews_test::WriteText(QueueFile(data), LinesText(150));
    CHECK(ews::PeekReports(data.wstring(), 100).value.lines.size() == 100);
    CHECK(ews::PeekReports(data.wstring(), 1000).value.lines.size() == 100);
    CHECK(ews::PeekReports(data.wstring(), 0).value.lines.size() == 1);
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(QueueDropsTheOldestWhenFull) {
    auto data = ews_test::NewDataFolder("queue-cap");
    ews_test::WriteText(QueueFile(data), LinesText(2016));
    auto result = ews::AppendReport(data.wstring(), "NEW1");
    CHECK(result.outcome == ews::QueueAppend::Added);
    CHECK(result.repairs.droppedOldest == 1);
    result = ews::AppendReport(data.wstring(), "NEW2");
    CHECK(result.repairs.droppedOldest == 1);
    auto all = ews::PeekReports(data.wstring(), 100);
    CHECK(ews_test::LineAt(all.value.lines, 0) == "L2");
    std::string text = ews_test::ReadText(QueueFile(data));
    CHECK(text.size() > 2000);
    CHECK(text.rfind("NEW2\n") == text.size() - 5);
    CHECK(text.rfind("NEW1\n") == text.size() - 10);
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(QueueSkipsABrokenLastLine) {
    auto data = ews_test::NewDataFolder("queue-broken-tail");
    ews_test::WriteText(QueueFile(data), "L0\nL1\n{\"half");
    auto peek = ews::PeekReports(data.wstring(), 10);
    CHECK(peek.ok());
    CHECK(peek.value.lines.size() == 2);
    CHECK(peek.value.repairs.skippedLines == 1);
    auto result = ews::AppendReport(data.wstring(), "L2");
    CHECK(result.outcome == ews::QueueAppend::Added);
    CHECK(result.repairs.skippedLines == 1);
    CHECK(ews_test::ReadText(QueueFile(data)) == "L0\nL1\nL2\n");
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(QueueSkipsDamagedLinesInTheMiddle) {
    auto data = ews_test::NewDataFolder("queue-damaged");
    ews_test::WriteText(QueueFile(data), "L0\n\nL1\r\n" + std::string(5000, 'x') + "\nL2\n");
    auto peek = ews::PeekReports(data.wstring(), 10);
    CHECK(peek.value.lines.size() == 2);
    CHECK(ews_test::LineAt(peek.value.lines, 0) == "L0");
    CHECK(ews_test::LineAt(peek.value.lines, 1) == "L2");
    CHECK(peek.value.repairs.skippedLines == 3);
    CHECK(ews::RemoveFirstReports(data.wstring(), 1).ok());
    CHECK(ews_test::ReadText(QueueFile(data)) == "L2\n");
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(QueueFileOver16MbIsRenamedAndANewQueueStarts) {
    auto data = ews_test::NewDataFolder("queue-oversize");
    ews_test::WriteText(QueueFile(data), std::string(ews::kMaxQueueFileBytes + 1, 'x'));
    auto peek = ews::PeekReports(data.wstring(), 10);
    CHECK(peek.ok());
    CHECK(peek.value.lines.empty());
    CHECK(peek.value.repairs.brokenFiles == 1);
    auto result = ews::AppendReport(data.wstring(), "FRESH");
    CHECK(result.outcome == ews::QueueAppend::Added);
    CHECK(result.repairs.brokenFiles == 1);
    CHECK(ews_test::ReadText(QueueFile(data)) == "FRESH\n");
    std::error_code ignored;
    CHECK(std::filesystem::file_size(data / "queue" / "reports.broken.jsonl", ignored) == ews::kMaxQueueFileBytes + 1);
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(QueueFileOfExactly16MbStillCounts) {
    auto data = ews_test::NewDataFolder("queue-16mb");
    std::string line(1023, 'x');
    std::string text;
    for (int count = 0; count < 16 * 1024; ++count) {
        text += line + "\n";
    }
    ews_test::WriteText(QueueFile(data), text);
    CHECK(text.size() == ews::kMaxQueueFileBytes);
    CHECK(ews::PeekReports(data.wstring(), 10).value.repairs.brokenFiles == 0);
    CHECK(ews::PeekReports(data.wstring(), 10).value.lines.size() == 10);
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(QueueFailsWhenTheDataFolderIsMissing) {
    auto data = ews_test::NewDataFolder("queue-nodata");
    CHECK(ews::AppendReport((data / "missing").wstring(), "L0").outcome == ews::QueueAppend::Failed);
    CHECK(!std::filesystem::exists(data / "missing"));
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(QueueFailsWhenTheQueueFolderCannotBeWritten) {
    auto data = ews_test::NewDataFolder("queue-notwritable");
    // A file named "queue" blocks the folder, so nothing can be written there.
    ews_test::WriteText(data / "queue", "not a folder");
    CHECK(ews::AppendReport(data.wstring(), "L0").outcome == ews::QueueAppend::Failed);
    CHECK(ews_test::ReadText(data / "queue") == "not a folder");
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(QueueFailedRewriteKeepsTheOldFile) {
    auto data = ews_test::NewDataFolder("queue-locked");
    AppendRange(data, 0, 2);
    // A reader that blocks renames over the queue file makes the atomic rewrite fail.
    HANDLE held = CreateFileW(QueueFile(data).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    auto result = ews::AppendReport(data.wstring(), "L2");
    CloseHandle(held);
    CHECK(result.outcome == ews::QueueAppend::Failed);
    CHECK(ews_test::ReadText(QueueFile(data)) == "L0\nL1\n");
    ews_test::RemoveDataFolder(data);
}
