// device_identity.cpp: the device id file `<data>\Client\device-id.txt` (see ews/device_identity.h).
// Used by ews-service at start. The format and the "never replace" rule are those of the client's DeviceIdentityStore.
#include "ews/device_identity.h"

#include <windows.h>
#include <objbase.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iterator>

#include "ews/file_io.h"
#include "ews/handles.h"

#pragma comment(lib, "ole32.lib")

namespace ews {

namespace {

constexpr std::size_t kMaxIdentityFileBytes = 256;
constexpr std::size_t kGuidLength = 36;

/// @return True for a space, tab or line end.
bool IsWhite(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

/// @return True for a hexadecimal digit.
bool IsHex(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

/// @return True for a character that is neither a hyphen nor a zero digit.
bool IsNonZeroDigit(char c) { return c != '-' && c != '0'; }

/// @return True when the character fits its place in "D" format: a hyphen at 8, 13, 18 and 23, a hex digit elsewhere.
bool FitsPlace(char c, std::size_t position) {
    bool hyphenPlace = position == 8 || position == 13 || position == 18 || position == 23;
    return hyphenPlace ? c == '-' : IsHex(c);
}

/// @return The lower case of one character (ASCII).
char LowerChar(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

/// Cuts white space at both ends. @return The trimmed copy.
std::string TrimWhite(const std::string& text) {
    auto first = std::find_if_not(text.begin(), text.end(), IsWhite);
    auto last = std::find_if_not(text.rbegin(), std::make_reverse_iterator(first), IsWhite).base();
    return std::string(first, last);
}

/// Reads the file and checks it. @return Ok with the identity; the failure of the read; or Error when the content is bad.
Reading<DeviceIdentity> ReadIdentity(const std::filesystem::path& file) {
    using Result = Reading<DeviceIdentity>;
    Reading<std::string> text = ReadWholeFile(file, kMaxIdentityFileBytes);
    if (!text.ok()) {
        return Result::Failure(text.status, text.error);
    }
    DeviceIdentity identity;
    if (!ParseDeviceId(text.value, identity.id)) {
        return Result::Failure(Status::Error, ERROR_INVALID_DATA);
    }
    return Result::Success(identity);
}

/// @return True when the failure says the file or its folder is confirmed not there.
bool IsConfirmedMissing(const Reading<DeviceIdentity>& read) {
    return !read.ok() && (read.error == ERROR_FILE_NOT_FOUND || read.error == ERROR_PATH_NOT_FOUND);
}

/// Makes a new random GUID in "D" format, lower case. @return The text, or an empty text when Windows cannot make one.
std::string NewGuidText() {
    GUID guid{};
    if (CoCreateGuid(&guid) != S_OK) {
        return std::string();
    }
    char buffer[40];
    std::snprintf(buffer, sizeof buffer, "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", guid.Data1, guid.Data2, guid.Data3, guid.Data4[0],
                  guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    return buffer;
}

/// Writes a new file that must not exist yet, and flushes it. The handle is closed when this returns.
Reading<bool> WriteNewFile(const std::filesystem::path& path, const std::string& bytes) {
    UniqueHandle file(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file.valid()) {
        return Reading<bool>::FromError(GetLastError());
    }
    DWORD written = 0;
    if (!WriteFile(file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) || written != bytes.size()) {
        return Reading<bool>::Failure(Status::Error, ERROR_WRITE_FAULT);
    }
    if (!FlushFileBuffers(file.get())) {
        return Reading<bool>::FromError(GetLastError());
    }
    return Reading<bool>::Success(true);
}

/// @return True for the errors that say the target name is already taken.
bool IsNameTaken(unsigned long error) { return error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS; }

/// Publishes a new identity: temporary file, then a rename that never replaces. When the name is taken
/// (the client or another process was faster) that identity is read and returned.
Reading<DeviceIdentity> CreateIdentity(const std::filesystem::path& folder, const std::filesystem::path& target) {
    using Result = Reading<DeviceIdentity>;
    std::string text = NewGuidText();
    if (text.empty()) {
        return Result::Failure(Status::Error, ERROR_GEN_FAILURE);
    }
    std::string compact = text;
    compact.erase(std::remove(compact.begin(), compact.end(), '-'), compact.end());
    std::filesystem::path temporary = folder / (".device-id-" + compact + ".tmp");
    Reading<bool> written = WriteNewFile(temporary, text + "\n");
    if (!written.ok()) {
        DeleteFileW(temporary.c_str());
        return Result::Failure(written.status, written.error);
    }
    BOOL moved = MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH);
    unsigned long error = moved ? 0 : GetLastError();
    DeleteFileW(temporary.c_str());
    if (moved) {
        DeviceIdentity identity;
        identity.id = text;
        identity.created = true;
        return Result::Success(identity);
    }
    return IsNameTaken(error) ? ReadIdentity(target) : Result::FromError(error);
}

}  // namespace

bool ParseDeviceId(const std::string& text, std::string& id) {
    std::string trimmed = TrimWhite(text);
    bool shapeOk = trimmed.size() == kGuidLength;
    for (std::size_t position = 0; position < trimmed.size(); ++position) {
        shapeOk = shapeOk && FitsPlace(trimmed[position], position);
    }
    bool notZero = shapeOk && std::any_of(trimmed.begin(), trimmed.end(), IsNonZeroDigit);
    if (!notZero) {
        return false;
    }
    id.resize(trimmed.size());
    std::transform(trimmed.begin(), trimmed.end(), id.begin(), LowerChar);
    return true;
}

Reading<DeviceIdentity> LoadOrCreateDeviceIdentity(const std::wstring& dataDir) {
    std::filesystem::path folder = std::filesystem::path(dataDir) / L"Client";
    std::filesystem::path file = folder / L"device-id.txt";
    Reading<DeviceIdentity> existing = ReadIdentity(file);
    // Only a confirmed absence permits creation. A bad or unreadable file is reported and never replaced.
    if (!IsConfirmedMissing(existing)) {
        return existing;
    }
    Reading<bool> made = EnsureSubfolder(dataDir, L"Client");
    if (!made.ok()) {
        return Reading<DeviceIdentity>::Failure(made.status, made.error);
    }
    return CreateIdentity(folder, file);
}

}  // namespace ews
