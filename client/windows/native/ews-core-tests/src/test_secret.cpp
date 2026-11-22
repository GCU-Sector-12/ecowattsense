// test_secret.cpp: tests of the server key rule and of the DPAPI protected key file.
#include <string>

#include "ews/secret.h"
#include "temp_folder.h"
#include "test_common.h"

namespace {

/// Checks that IsValidServerKey gives the expected answer for one key.
void CheckKey(const std::string& key, bool expected) {
    if (ews::IsValidServerKey(key) != expected) {
        ews_test::Check(false, "key rule", __FILE__, __LINE__);
    }
}

}  // namespace

EWS_TEST(SecretKeyRule) {
    CheckKey("a", true);
    CheckKey("test-key", true);
    CheckKey("abc def", true);
    CheckKey("!~", true);
    CheckKey(std::string(256, 'k'), true);
    CheckKey(std::string(257, 'k'), false);
    CheckKey("", false);
    CheckKey(" abc", false);
    CheckKey("abc ", false);
    CheckKey(" ", false);
    CheckKey("ab\ncd", false);
    CheckKey("ab\rcd", false);
    CheckKey("ab\tcd", false);
    CheckKey(std::string("ab\0cd", 5), false);
    CheckKey("ab\x7f", false);
    CheckKey("abc\xc3\xa9", false);
}

EWS_TEST(SecretProtectAndUnprotectRoundTrip) {
    auto blob = ews::ProtectBytes("my-secret-key");
    CHECK(blob.ok());
    CHECK(blob.value != "my-secret-key");
    CHECK(blob.value.find("my-secret-key") == std::string::npos);
    auto plain = ews::UnprotectBytes(blob.value);
    CHECK(plain.ok());
    CHECK(plain.value == "my-secret-key");
}

EWS_TEST(SecretUnprotectRefusesGarbage) {
    CHECK(!ews::UnprotectBytes("this is not a blob").ok());
    CHECK(!ews::UnprotectBytes("").ok());
}

EWS_TEST(SecretStoreAndLoadKeyFile) {
    auto data = ews_test::NewDataFolder("secret-file");
    CHECK(ews::StoreServerKey(data.wstring(), "server-key-123").ok());
    std::string onDisk = ews_test::ReadText(data / "config" / "key.bin");
    CHECK(!onDisk.empty());
    CHECK(onDisk.find("server-key-123") == std::string::npos);
    auto loaded = ews::LoadServerKey(data.wstring());
    CHECK(loaded.ok());
    CHECK(loaded.value == "server-key-123");
    CHECK(ews::StoreServerKey(data.wstring(), "second-key").ok());
    CHECK(ews::LoadServerKey(data.wstring()).value == "second-key");
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(SecretMissingFileIsUnavailable) {
    auto data = ews_test::NewDataFolder("secret-missing");
    CHECK(ews::LoadServerKey(data.wstring()).status == ews::Status::Unavailable);
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(SecretDamagedFileIsAnError) {
    auto data = ews_test::NewDataFolder("secret-damaged");
    CHECK(ews::StoreServerKey(data.wstring(), "server-key-123").ok());
    std::string blob = ews_test::ReadText(data / "config" / "key.bin");
    blob.resize(blob.size() / 2);
    ews_test::WriteText(data / "config" / "key.bin", blob);
    auto damaged = ews::LoadServerKey(data.wstring());
    CHECK(damaged.status == ews::Status::Error);
    CHECK(damaged.value.empty());
    ews_test::WriteText(data / "config" / "key.bin", blob + "junk");
    CHECK(ews::LoadServerKey(data.wstring()).status == ews::Status::Error);
    ews_test::WriteText(data / "config" / "key.bin", "not a protected blob");
    CHECK(ews::LoadServerKey(data.wstring()).status == ews::Status::Error);
    ews_test::WriteText(data / "config" / "key.bin", "");
    CHECK(ews::LoadServerKey(data.wstring()).status == ews::Status::Error);
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(SecretProtectedBadKeyIsAnError) {
    auto data = ews_test::NewDataFolder("secret-badkey");
    auto blob = ews::ProtectBytes("bad key ");
    CHECK(blob.ok());
    ews_test::WriteText(data / "config" / "key.bin", blob.value);
    CHECK(ews::LoadServerKey(data.wstring()).status == ews::Status::Error);
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(SecretStoreRefusesABadKeyAndAMissingDataFolder) {
    auto data = ews_test::NewDataFolder("secret-refuse");
    CHECK(!ews::StoreServerKey(data.wstring(), "bad key ").ok());
    CHECK(!std::filesystem::exists(data / "config" / "key.bin"));
    CHECK(!ews::StoreServerKey((data / "missing").wstring(), "good-key").ok());
    ews_test::RemoveDataFolder(data);
}
