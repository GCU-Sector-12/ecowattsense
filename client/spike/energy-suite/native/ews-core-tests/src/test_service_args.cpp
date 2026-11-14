// test_service_args.cpp: tests of the ews-service command line parser (SPEC 5.2) and of the single-instance lock.
#include <string>
#include <vector>

#include "ews/instance_lock.h"
#include "ews/service_args.h"
#include "temp_folder.h"
#include "test_common.h"

namespace {

/// Parses a list of arguments.
ews::ServiceArgsResult Parse(const std::vector<std::wstring>& arguments) { return ews::ParseServiceArgs(arguments); }

}  // namespace

EWS_TEST(ServiceArgsNoArgumentIsTheServiceMode) {
    auto r = Parse({});
    CHECK(r.ok);
    CHECK(r.args.mode == ews::ServiceMode::Scm);
    CHECK(r.args.dataDir == ews::kDefaultDataDir);
    CHECK(r.args.tickSeconds == 60);
    CHECK(r.args.runSeconds == 0);
}

EWS_TEST(ServiceArgsConsoleWithAllOptions) {
    auto r = Parse({L"--console", L"--data-dir", L"C:\\x y", L"--tick-seconds", L"2", L"--run-seconds", L"34"});
    CHECK(r.ok);
    CHECK(r.args.mode == ews::ServiceMode::Console);
    CHECK(r.args.dataDir == L"C:\\x y");
    CHECK(r.args.tickSeconds == 2);
    CHECK(r.args.runSeconds == 34);
}

EWS_TEST(ServiceArgsConfigureWithDataDir) {
    auto r = Parse({L"--configure", L"--data-dir", L"D:\\data"});
    CHECK(r.ok);
    CHECK(r.args.mode == ews::ServiceMode::Configure);
    CHECK(r.args.dataDir == L"D:\\data");
}

EWS_TEST(ServiceArgsDataDirAloneKeepsTheServiceMode) {
    auto r = Parse({L"--data-dir", L"D:\\data"});
    CHECK(r.ok);
    CHECK(r.args.mode == ews::ServiceMode::Scm);
}

EWS_TEST(ServiceArgsRefuseWhatIsNotAllowed) {
    CHECK(!Parse({L"--bogus"}).ok);
    CHECK(!Parse({L"--console", L"--configure"}).ok);
    CHECK(!Parse({L"--console", L"--console"}).ok);
    CHECK(!Parse({L"--data-dir"}).ok);
    CHECK(!Parse({L"--data-dir", L""}).ok);
    CHECK(!Parse({L"--data-dir", L"--console"}).ok);
    CHECK(!Parse({L"--data-dir", L"a", L"--data-dir", L"b"}).ok);
    CHECK(!Parse({L"--tick-seconds", L"2"}).ok);
    CHECK(!Parse({L"--configure", L"--run-seconds", L"2"}).ok);
    CHECK(!Parse({L"--console", L"--tick-seconds", L"0"}).ok);
    CHECK(!Parse({L"--console", L"--tick-seconds", L"61"}).ok);
    CHECK(!Parse({L"--console", L"--tick-seconds", L"-1"}).ok);
    CHECK(!Parse({L"--console", L"--tick-seconds", L"2x"}).ok);
    CHECK(!Parse({L"--console", L"--tick-seconds", L"\x0662"}).ok);
    CHECK(!Parse({L"--console", L"--run-seconds", L"0"}).ok);
    CHECK(!Parse({L"--console", L"--run-seconds", L"86401"}).ok);
    CHECK(!Parse({L"--console", L"--run-seconds"}).ok);
    CHECK(!Parse({L"stray"}).ok);
}

EWS_TEST(InstanceLockIsExclusiveAndReleases) {
    auto folder = ews_test::NewDataFolder("lock");
    ews::InstanceLock first;
    ews::InstanceLock second;
    CHECK(first.Acquire(folder.wstring()) == ews::LockState::Taken);
    CHECK(second.Acquire(folder.wstring()) == ews::LockState::Held);
    CHECK(second.error() == ERROR_SHARING_VIOLATION);
    first.Release();
    CHECK(second.Acquire(folder.wstring()) == ews::LockState::Taken);
    second.Release();
    ews_test::RemoveDataFolder(folder);
}

EWS_TEST(InstanceLockFailsWithoutADataFolder) {
    ews::InstanceLock lock;
    CHECK(lock.Acquire(L"C:\\this\\folder\\does\\not\\exist") == ews::LockState::Failed);
}
