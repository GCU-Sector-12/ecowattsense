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
