// test_scan_args.cpp: tests of the ews-scan command line parser (SPEC 3.1).
#include <string>
#include <vector>

#include "ews/scan_args.h"
#include "test_common.h"

using ews::ScanContext;

namespace {

/// Parses a list of arguments.
ews::ScanArgsResult Parse(const std::vector<std::wstring>& arguments) { return ews::ParseScanArgs(arguments); }

}  // namespace

EWS_TEST(ArgsNoneGivesDefaults) {
    auto r = Parse({});
    CHECK(r.ok);
    CHECK(r.args.outFolder.empty());
    CHECK(!r.args.context.has_value());
    CHECK(!r.args.quiet);
}

EWS_TEST(ArgsOutFolder) {
    auto r = Parse({L"--out", L"x"});
    CHECK(r.ok);
    CHECK(r.args.outFolder == L"x");
}

EWS_TEST(ArgsContextService) {
    auto r = Parse({L"--context", L"service"});
    CHECK(r.ok);
    CHECK(r.args.context.has_value());
    CHECK(r.args.context.value_or(ScanContext::User) == ScanContext::Service);
}

EWS_TEST(ArgsContextUserAndAdmin) {
    CHECK(Parse({L"--context", L"user"}).args.context.value_or(ScanContext::Service) == ScanContext::User);
    CHECK(Parse({L"--context", L"admin"}).args.context.value_or(ScanContext::Service) == ScanContext::Admin);
}

EWS_TEST(ArgsQuiet) {
    auto r = Parse({L"--quiet"});
    CHECK(r.ok);
    CHECK(r.args.quiet);
}

EWS_TEST(ArgsAllTogether) {
    auto r = Parse({L"--quiet", L"--out", L"C:\\data\\scan", L"--context", L"admin"});
    CHECK(r.ok);
    CHECK(r.args.quiet);
    CHECK(r.args.outFolder == L"C:\\data\\scan");
    CHECK(r.args.context.value_or(ScanContext::User) == ScanContext::Admin);
}

EWS_TEST(ArgsBadContextIsRejected) {
    CHECK(!Parse({L"--context", L"nonsense"}).ok);
    CHECK(!Parse({L"--context", L"USER"}).ok);
}

EWS_TEST(ArgsUnknownFlagIsRejected) {
    CHECK(!Parse({L"--verbose"}).ok);
    CHECK(!Parse({L"stray"}).ok);
    CHECK(!Parse({L"--quiet", L"--other"}).ok);
}

EWS_TEST(ArgsOptionWithoutValueIsRejected) {
    CHECK(!Parse({L"--out"}).ok);
    CHECK(!Parse({L"--context"}).ok);
    CHECK(!Parse({L"--quiet", L"--out"}).ok);
}

EWS_TEST(ArgsWatchAlone) {
    auto r = Parse({L"--watch"});
    CHECK(r.ok);
    CHECK(r.args.watch);
    CHECK(r.args.periodMs == 2000);
}

EWS_TEST(ArgsWatchWithAPeriod) {
    auto r = Parse({L"--watch", L"--period-ms", L"500"});
    CHECK(r.ok && r.args.watch && r.args.periodMs == 500);
    auto other = Parse({L"--period-ms", L"10000", L"--watch"});
    CHECK(other.ok && other.args.watch && other.args.periodMs == 10000);
    CHECK(Parse({L"--watch", L"--period-ms", L"2500"}).args.periodMs == 2500);
}

EWS_TEST(ArgsWatchPeriodOutsideTheRangeIsRejected) {
    CHECK(!Parse({L"--watch", L"--period-ms", L"499"}).ok);
    CHECK(!Parse({L"--watch", L"--period-ms", L"10001"}).ok);
    CHECK(!Parse({L"--watch", L"--period-ms", L"0"}).ok);
    CHECK(!Parse({L"--watch", L"--period-ms", L"99999999999"}).ok);
}

EWS_TEST(ArgsWatchPeriodMustBeDigitsOnly) {
    CHECK(!Parse({L"--watch", L"--period-ms", L"2e3"}).ok);
    CHECK(!Parse({L"--watch", L"--period-ms", L"-1000"}).ok);
    CHECK(!Parse({L"--watch", L"--period-ms", L"+1000"}).ok);
    CHECK(!Parse({L"--watch", L"--period-ms", L" 1000"}).ok);
    CHECK(!Parse({L"--watch", L"--period-ms", L"abc"}).ok);
    CHECK(!Parse({L"--watch", L"--period-ms"}).ok);
}

EWS_TEST(ArgsPeriodWithoutWatchIsRejected) {
    CHECK(!Parse({L"--period-ms", L"1000"}).ok);
}

EWS_TEST(ArgsWatchCannotBeCombinedWithOutContextOrQuiet) {
    CHECK(!Parse({L"--watch", L"--out", L"x"}).ok);
    CHECK(!Parse({L"--out", L"x", L"--watch"}).ok);
    CHECK(!Parse({L"--watch", L"--context", L"user"}).ok);
    CHECK(!Parse({L"--quiet", L"--watch"}).ok);
}

EWS_TEST(ArgsWithoutWatchKeepTheDefaults) {
    auto r = Parse({L"--quiet"});
    CHECK(!r.args.watch);
    CHECK(r.args.periodMs == 2000);
}
