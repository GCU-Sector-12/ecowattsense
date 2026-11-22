// test_atomic_file.cpp: tests of WriteFileAtomically on a folder below the temporary folder.
#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "ews/atomic_file.h"
#include "test_common.h"

namespace {

/// Makes a new empty folder for one test. @return Its path.
std::filesystem::path NewFolder(const char* name) {
    auto folder = std::filesystem::temp_directory_path() / (std::string("ews-test-") + name + "-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::remove_all(folder);
    std::filesystem::create_directory(folder);
    return folder;
}

/// @return The whole content of a file.
std::string ReadAll(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

}  // namespace

EWS_TEST(AtomicFileWritesAndReplaces) {
    auto folder = NewFolder("write");
    auto first = ews::WriteFileAtomically(folder.wstring(), L"a.json", "one");
    CHECK(first.ok());
    CHECK(ReadAll(folder / "a.json") == "one");
    auto second = ews::WriteFileAtomically(folder.wstring(), L"a.json", "two!");
    CHECK(second.ok());
    CHECK(ReadAll(folder / "a.json") == "two!");
    CHECK(!std::filesystem::exists(folder / "a.json.tmp"));
    std::filesystem::remove_all(folder);
}

EWS_TEST(AtomicFileFailsForAMissingFolderAndLeavesNothing) {
    auto folder = NewFolder("missing");
    auto missing = folder / "nope";
    auto result = ews::WriteFileAtomically(missing.wstring(), L"a.json", "x");
    CHECK(!result.ok());
    CHECK(!std::filesystem::exists(missing));
    std::filesystem::remove_all(folder);
}

EWS_TEST(AtomicFileKeepsTheOldFileWhenTheTargetIsOpen) {
    auto folder = NewFolder("open");
    CHECK(ews::WriteFileAtomically(folder.wstring(), L"a.json", "old").ok());
    auto target = folder / "a.json";
    HANDLE held = CreateFileW(target.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    auto result = ews::WriteFileAtomically(folder.wstring(), L"a.json", "new");
    CloseHandle(held);
    CHECK(!result.ok());
    CHECK(result.error != 0);
    CHECK(ReadAll(target) == "old");
    CHECK(!std::filesystem::exists(folder / "a.json.tmp"));
    std::filesystem::remove_all(folder);
}
