// secret.cpp: DPAPI protection of the server key and the key.bin file (see ews/secret.h).
// Used by ews-service and by the configure input.
#include "ews/secret.h"

#include <windows.h>
#include <wincrypt.h>

#include <algorithm>
#include <filesystem>

#include "ews/atomic_file.h"
#include "ews/file_io.h"

#pragma comment(lib, "crypt32.lib")

namespace ews {

namespace {

constexpr std::size_t kMaxBlobBytes = 8 * 1024;       // key.bin is far smaller; a larger file is not a key file.
constexpr std::size_t kMaxProtectedBytes = 64 * 1024;  // The DPAPI input is a key, never more than this.

/// @return True for a printable ASCII character (space included).
bool IsPrintableAscii(char c) { return c >= 0x20 && c <= 0x7e; }

/// Copies the bytes of a blob that DPAPI allocated, wipes them and frees the memory.
/// @param blob The output of CryptProtectData or CryptUnprotectData; it is empty afterwards.
/// @return The copied bytes.
std::string TakeBlob(DATA_BLOB& blob) {
    std::string bytes(reinterpret_cast<const char*>(blob.pbData), blob.cbData);
    SecureZeroMemory(blob.pbData, blob.cbData);
    LocalFree(blob.pbData);
    blob.pbData = nullptr;
    blob.cbData = 0;
    return bytes;
}

}  // namespace

bool IsValidServerKey(const std::string& key) {
    bool lengthOk = !key.empty() && key.size() <= kMaxServerKeyLength;
    bool edgesOk = lengthOk && key.front() != ' ' && key.back() != ' ';
    return edgesOk && std::all_of(key.begin(), key.end(), IsPrintableAscii);
}

Reading<std::string> ProtectBytes(const std::string& plain) {
    using Result = Reading<std::string>;
    if (plain.empty() || plain.size() > kMaxProtectedBytes) {
        return Result::Failure(Status::Error, ERROR_INVALID_PARAMETER);
    }
    std::string copy = plain;
    DATA_BLOB input{static_cast<DWORD>(copy.size()), reinterpret_cast<BYTE*>(copy.data())};
    DATA_BLOB output{};
    BOOL done = CryptProtectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN | CRYPTPROTECT_LOCAL_MACHINE, &output);
    DWORD error = GetLastError();
    SecureZeroMemory(copy.data(), copy.size());
    if (!done) {
        return Result::FromError(error);
    }
    return Result::Success(TakeBlob(output));
}

Reading<std::string> UnprotectBytes(const std::string& blob) {
    using Result = Reading<std::string>;
    if (blob.empty() || blob.size() > kMaxBlobBytes) {
        return Result::Failure(Status::Error, ERROR_INVALID_DATA);
    }
    std::string copy = blob;
    DATA_BLOB input{static_cast<DWORD>(copy.size()), reinterpret_cast<BYTE*>(copy.data())};
    DATA_BLOB output{};
    BOOL done = CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output);
    DWORD error = GetLastError();
    if (!done) {
        // Any failure here means the blob is damaged or from another computer: an Error, never "nothing exists".
        return Result::Failure(Status::Error, error);
    }
    return Result::Success(TakeBlob(output));
}

Reading<bool> StoreServerKey(const std::wstring& dataDir, const std::string& key) {
    if (!IsValidServerKey(key)) {
        return Reading<bool>::Failure(Status::Error, ERROR_INVALID_DATA);
    }
    Reading<bool> folder = EnsureSubfolder(dataDir, L"config");
    if (!folder.ok()) {
        return folder;
    }
    Reading<std::string> blob = ProtectBytes(key);
    if (!blob.ok()) {
        return Reading<bool>::Failure(blob.status, blob.error);
    }
    std::filesystem::path configFolder = std::filesystem::path(dataDir) / L"config";
    return WriteFileAtomically(configFolder.wstring(), L"key.bin", blob.value);
}

Reading<std::string> LoadServerKey(const std::wstring& dataDir) {
    using Result = Reading<std::string>;
    std::filesystem::path file = std::filesystem::path(dataDir) / L"config" / L"key.bin";
    Reading<std::string> blob = ReadWholeFile(file, kMaxBlobBytes);
    if (!blob.ok()) {
        return blob;
    }
    Reading<std::string> plain = UnprotectBytes(blob.value);
    if (!plain.ok()) {
        return plain;
    }
    if (!IsValidServerKey(plain.value)) {
        SecureZeroMemory(plain.value.data(), plain.value.size());
        return Result::Failure(Status::Error, ERROR_INVALID_DATA);
    }
    return plain;
}

}  // namespace ews
