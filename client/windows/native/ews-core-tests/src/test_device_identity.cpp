// test_device_identity.cpp: tests of the device id file (format, create-new, never replace a bad file).
#include <string>

#include "ews/device_identity.h"
#include "temp_folder.h"
#include "test_common.h"

namespace {

const char* const kGuid = "3f2a9c1e-5b7d-4e8a-9c6f-1a2b3c4d5e6f";

/// Checks that ParseDeviceId gives the expected answer for one text.
void CheckParse(const std::string& text, bool expected) {
    std::string id;
    if (ews::ParseDeviceId(text, id) != expected) {
        ews_test::Check(false, "device id rule", __FILE__, __LINE__);
    }
}

/// @return The path of the identity file below a data folder.
std::filesystem::path IdFile(const std::filesystem::path& data) { return data / "Client" / "device-id.txt"; }

}  // namespace

EWS_TEST(DeviceIdParseRule) {
    CheckParse(kGuid, true);
    CheckParse(std::string(kGuid) + "\n", true);
    CheckParse(std::string(kGuid) + "\r\n", true);
    CheckParse(std::string("  ") + kGuid + " \n", true);
    CheckParse("3F2A9C1E-5B7D-4E8A-9C6F-1A2B3C4D5E6F", true);
    CheckParse("00000000-0000-0000-0000-000000000000", false);
    CheckParse("", false);
    CheckParse("3f2a9c1e5b7d4e8a9c6f1a2b3c4d5e6f", false);
    CheckParse("{3f2a9c1e-5b7d-4e8a-9c6f-1a2b3c4d5e6f}", false);
    CheckParse("3f2a9c1e-5b7d-4e8a-9c6f-1a2b3c4d5e6", false);
    CheckParse("3f2a9c1e-5b7d-4e8a-9c6f-1a2b3c4d5e6g", false);
    CheckParse(std::string(kGuid) + "\nextra", false);
    CheckParse(std::string(kGuid) + kGuid, false);
}

EWS_TEST(DeviceIdParseNormalisesToLowerCase) {
    std::string id;
    CHECK(ews::ParseDeviceId("3F2A9C1E-5B7D-4E8A-9C6F-1A2B3C4D5E6F\n", id));
    CHECK(id == kGuid);
}

EWS_TEST(DeviceIdIsCreatedInTheClientFormat) {
    auto data = ews_test::NewDataFolder("identity-create");
    auto first = ews::LoadOrCreateDeviceIdentity(data.wstring());
    CHECK(first.ok());
    CHECK(first.value.created);
    std::string text = ews_test::ReadText(IdFile(data));
    CHECK(text.size() == 37);
    CHECK(text.back() == '\n');
    std::string parsed;
    CHECK(ews::ParseDeviceId(text, parsed));
    CHECK(parsed == first.value.id);
    auto second = ews::LoadOrCreateDeviceIdentity(data.wstring());
    CHECK(second.ok());
    CHECK(!second.value.created);
    CHECK(second.value.id == first.value.id);
    CHECK(ews_test::ReadText(IdFile(data)) == text);
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(DeviceIdExistingFileIsNeverOverwritten) {
    auto data = ews_test::NewDataFolder("identity-existing");
    ews_test::WriteText(IdFile(data), std::string("3F2A9C1E-5B7D-4E8A-9C6F-1A2B3C4D5E6F\n"));
    auto result = ews::LoadOrCreateDeviceIdentity(data.wstring());
    CHECK(result.ok());
    CHECK(!result.value.created);
    CHECK(result.value.id == kGuid);
    CHECK(ews_test::ReadText(IdFile(data)) == "3F2A9C1E-5B7D-4E8A-9C6F-1A2B3C4D5E6F\n");
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(DeviceIdBadFileIsAnErrorAndStaysAsItIs) {
    auto data = ews_test::NewDataFolder("identity-bad");
    const char* bad[] = {"not a guid\n", "00000000-0000-0000-0000-000000000000\n", "", "3f2a9c1e-5b7d-4e8a-9c6f-1a2b3c4d5e6f\nmore\n"};
    for (const char* content : bad) {
        ews_test::WriteText(IdFile(data), content);
        auto result = ews::LoadOrCreateDeviceIdentity(data.wstring());
        CHECK(!result.ok());
        CHECK(result.status == ews::Status::Error);
        CHECK(ews_test::ReadText(IdFile(data)) == content);
    }
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(DeviceIdFileOver256BytesIsBad) {
    auto data = ews_test::NewDataFolder("identity-big");
    ews_test::WriteText(IdFile(data), std::string(kGuid) + std::string(300, ' '));
    CHECK(!ews::LoadOrCreateDeviceIdentity(data.wstring()).ok());
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(DeviceIdMissingDataFolderIsAFailure) {
    auto data = ews_test::NewDataFolder("identity-nodata");
    CHECK(!ews::LoadOrCreateDeviceIdentity((data / "missing").wstring()).ok());
    CHECK(!std::filesystem::exists(data / "missing"));
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(DeviceIdLeavesNoTemporaryFile) {
    auto data = ews_test::NewDataFolder("identity-tmp");
    CHECK(ews::LoadOrCreateDeviceIdentity(data.wstring()).ok());
    int files = 0;
    std::error_code ignored;
    for (const auto& entry : std::filesystem::directory_iterator(data / "Client", ignored)) {
        files += entry.is_regular_file() ? 1 : 0;
    }
    CHECK(files == 1);
    ews_test::RemoveDataFolder(data);
}

EWS_TEST(DeviceIdRenameOverAnExistingFileIsRefused) {
    auto data = ews_test::NewDataFolder("identity-race");
    // A reader that holds the file open without delete sharing must not break the creation of nothing: the file exists.
    ews_test::WriteText(IdFile(data), std::string(kGuid) + "\n");
    HANDLE held = CreateFileW(IdFile(data).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    auto result = ews::LoadOrCreateDeviceIdentity(data.wstring());
    CloseHandle(held);
    CHECK(result.ok());
    CHECK(result.value.id == kGuid);
    ews_test::RemoveDataFolder(data);
}
