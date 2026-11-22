// test_diag_recorder.cpp: tests of the rules around the diagnostics file (ews/diag_recorder.h, SPEC 7a):
// file names, the 7 day rule, the 16 KB rule, the full file, and the counting of failed writes. A fake writer is used.
#include <chrono>
#include <string>

#include "ews/diag_recorder.h"
#include "fake_diag.h"
#include "test_common.h"

namespace {

/// @return A UTC time on the given day of November 2026 at the given hour.
ews::UtcTime At(int day, int hour = 10) {
    return std::chrono::sys_days{std::chrono::year{2026} / 11 / day} + std::chrono::hours(hour);
}

}  // namespace

EWS_TEST(DiagFileNameUsesTheUtcDate) {
    CHECK(ews::DiagFileName(At(14)) == L"diag-2026-11-14.jsonl");
    CHECK(ews::DiagFileName(At(14, 23) + std::chrono::minutes(59)) == L"diag-2026-11-14.jsonl");
    CHECK(ews::DiagFileName(At(15, 0)) == L"diag-2026-11-15.jsonl");
}

EWS_TEST(DiagFileNameRule) {
    CHECK(ews::IsDiagFileName(L"diag-2026-11-14.jsonl"));
    CHECK(!ews::IsDiagFileName(L"diag-2026-13-14.jsonl"));
    CHECK(!ews::IsDiagFileName(L"diag-2026-02-30.jsonl"));
    CHECK(!ews::IsDiagFileName(L"diag-2026-11-14.csv"));
    CHECK(!ews::IsDiagFileName(L"xdiag-2026-11-14.jsonl"));
    CHECK(!ews::IsDiagFileName(L"diag-2026-11-14.jsonl.bak"));
    CHECK(!ews::IsDiagFileName(L"service.log"));
    CHECK(!ews::IsDiagFileName(L"diag-2026-11-1\x0130.jsonl"));
}

EWS_TEST(DiagFilesOlderThanSevenDaysAreOld) {
    ews::UtcTime now = At(14);
    CHECK(!ews::DiagFileIsOld(L"diag-2026-11-14.jsonl", now));
    CHECK(!ews::DiagFileIsOld(L"diag-2026-11-07.jsonl", now));
    CHECK(ews::DiagFileIsOld(L"diag-2026-11-06.jsonl", now));
    CHECK(ews::DiagFileIsOld(L"diag-2025-01-01.jsonl", now));
    CHECK(!ews::DiagFileIsOld(L"diag-2026-11-20.jsonl", now));
    CHECK(!ews::DiagFileIsOld(L"service.log", now));
    CHECK(!ews::DiagFileIsOld(L"diag-2020-01-01.jsonl.bak", now));
}

EWS_TEST(DiagRecorderWithoutAWriterDoesNothing) {
    ews::DiagRecorder recorder(nullptr);
    ews::DiagOutcome outcome = recorder.Record(At(14), "{}");
    CHECK(outcome.note == ews::DiagNote::None);
    CHECK(recorder.Failures() == 0);
    recorder.Clean(At(14));
}

EWS_TEST(DiagRecorderWritesALine) {
    ews_test::FakeDiagWriter writer;
    ews::DiagRecorder recorder(&writer);
    ews::DiagOutcome outcome = recorder.Record(At(14), "{\"t\":\"tick\"}");
    CHECK(outcome.note == ews::DiagNote::None);
    CHECK(writer.lines.size() == 1);
    CHECK(writer.lines.empty() || writer.lines[0] == "{\"t\":\"tick\"}");
    CHECK(recorder.Failures() == 0);
}

EWS_TEST(DiagRecorderRefusesALineThatIsTooLongAndCountsIt) {
    ews_test::FakeDiagWriter writer;
    ews::DiagRecorder recorder(&writer);
    ews::DiagOutcome first = recorder.Record(At(14), std::string(16384, 'a'));
    CHECK(writer.appends == 0);
    CHECK(first.note == ews::DiagNote::WriteFailed && first.failures == 1);
    ews::DiagOutcome second = recorder.Record(At(14) + std::chrono::minutes(1), std::string(20000, 'a'));
    CHECK(second.note == ews::DiagNote::None);
    CHECK(recorder.Failures() == 2);
    ews::DiagOutcome fine = recorder.Record(At(14), std::string(16383, 'a'));
    CHECK(fine.note == ews::DiagNote::None);
    CHECK(writer.appends == 1);
}

EWS_TEST(DiagRecorderRefusesAnEmptyLine) {
    ews_test::FakeDiagWriter writer;
    ews::DiagRecorder recorder(&writer);
    ews::DiagOutcome outcome = recorder.Record(At(14), "");
    CHECK(writer.appends == 0);
    CHECK(outcome.note == ews::DiagNote::WriteFailed && outcome.failures == 1);
}

EWS_TEST(DiagRecorderReportsTheFirstFailureAndThenOneAnHour) {
    ews_test::FakeDiagWriter writer;
    writer.next = ews::DiagWriteResult::Failed;
    ews::DiagRecorder recorder(&writer);
    ews::UtcTime start = At(14);
    ews::DiagOutcome first = recorder.Record(start, "{}");
    CHECK(first.note == ews::DiagNote::WriteFailed && first.failures == 1);
    ews::DiagOutcome second = recorder.Record(start + std::chrono::minutes(1), "{}");
    CHECK(second.note == ews::DiagNote::None);
    ews::DiagOutcome almost = recorder.Record(start + std::chrono::minutes(59), "{}");
    CHECK(almost.note == ews::DiagNote::None);
    ews::DiagOutcome hour = recorder.Record(start + std::chrono::hours(1), "{}");
    CHECK(hour.note == ews::DiagNote::WriteFailed && hour.failures == 4);
    CHECK(recorder.Failures() == 4);
    CHECK(writer.appends == 4);
}

EWS_TEST(DiagRecorderKeepsWritingAfterAFailure) {
    ews_test::FakeDiagWriter writer;
    writer.next = ews::DiagWriteResult::Failed;
    ews::DiagRecorder recorder(&writer);
    recorder.Record(At(14), "{}");
    writer.next = ews::DiagWriteResult::Written;
    ews::DiagOutcome outcome = recorder.Record(At(14) + std::chrono::minutes(1), "{\"t\":\"tick\"}");
    CHECK(outcome.note == ews::DiagNote::None);
    CHECK(writer.lines.size() == 1);
}

EWS_TEST(DiagRecorderFullFileIsReportedOnceAndStopsTheDay) {
    ews_test::FakeDiagWriter writer;
    writer.next = ews::DiagWriteResult::Full;
    ews::DiagRecorder recorder(&writer);
    ews::DiagOutcome first = recorder.Record(At(14), "{}");
    CHECK(first.note == ews::DiagNote::Full);
    CHECK(writer.appends == 1);
    ews::DiagOutcome second = recorder.Record(At(14, 11), "{}");
    CHECK(second.note == ews::DiagNote::None);
    CHECK(writer.appends == 1);  // nothing more is tried that day
    CHECK(recorder.Failures() == 0);
    writer.next = ews::DiagWriteResult::Written;
    ews::DiagOutcome nextDay = recorder.Record(At(15, 0), "{}");
    CHECK(nextDay.note == ews::DiagNote::None);
    CHECK(writer.lines.size() == 1);
}

EWS_TEST(DiagRecorderCleanPassesTheTimeOn) {
    ews_test::FakeDiagWriter writer;
    ews::DiagRecorder recorder(&writer);
    recorder.Clean(At(14));
    CHECK(writer.cleans == 1);
    CHECK(writer.lastClean == At(14));
}
