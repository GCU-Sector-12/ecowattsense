// web_setup_hash.cpp: the SHA256 of a file with BCrypt, for the web installer (ews/web_setup.h).
#include <windows.h>
#include <bcrypt.h>

#include <string>
#include <vector>

#include "ews/handles.h"
#include "ews/web_setup.h"

#pragma comment(lib, "bcrypt.lib")

namespace ews {

namespace {

/// Owns a BCrypt algorithm provider handle.
class AlgorithmHandle {
public:
    AlgorithmHandle() = default;
    AlgorithmHandle(const AlgorithmHandle&) = delete;
    AlgorithmHandle& operator=(const AlgorithmHandle&) = delete;
    ~AlgorithmHandle() { Close(); }

    /// Opens the SHA256 provider. @return The NTSTATUS (0 = success).
    NTSTATUS Open() { return BCryptOpenAlgorithmProvider(&handle_, BCRYPT_SHA256_ALGORITHM, nullptr, 0); }
    /// @return The raw handle.
    BCRYPT_ALG_HANDLE get() const { return handle_; }

private:
    /// Closes the provider when one is open.
    void Close() {
        if (handle_ != nullptr) {
            BCryptCloseAlgorithmProvider(handle_, 0);
        }
    }
    BCRYPT_ALG_HANDLE handle_ = nullptr;
};

/// Owns a BCrypt hash object handle.
class HashHandle {
public:
    HashHandle() = default;
    HashHandle(const HashHandle&) = delete;
    HashHandle& operator=(const HashHandle&) = delete;
    ~HashHandle() { Close(); }

    /// Creates a hash object. @param algorithm The provider. @return The NTSTATUS (0 = success).
    NTSTATUS Create(BCRYPT_ALG_HANDLE algorithm) { return BCryptCreateHash(algorithm, &handle_, nullptr, 0, nullptr, 0, 0); }
    /// @return The raw handle.
    BCRYPT_HASH_HANDLE get() const { return handle_; }

private:
    /// Destroys the hash object when one exists.
    void Close() {
        if (handle_ != nullptr) {
            BCryptDestroyHash(handle_);
        }
    }
    BCRYPT_HASH_HANDLE handle_ = nullptr;
};

enum class Step { More, Done, Failed };

/// Reads one chunk of the file and feeds it to the hash.
/// @param file The open file.
/// @param hash The hash object.
/// @param buffer A reusable buffer.
/// @param error Receives the Windows error on failure.
/// @return More while data arrives, Done at the end of the file, Failed on an error.
Step HashStep(HANDLE file, BCRYPT_HASH_HANDLE hash, std::vector<UCHAR>& buffer, unsigned long& error) {
    DWORD read = 0;
    if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) {
        error = GetLastError();
        return Step::Failed;
    }
    if (read == 0) {
        return Step::Done;
    }
    NTSTATUS status = BCryptHashData(hash, buffer.data(), read, 0);
    if (status != 0) {
        error = ERROR_INVALID_DATA;
        return Step::Failed;
    }
    return Step::More;
}

/// @param digest The hash bytes.
/// @return Lower-case hex digits, two for each byte.
std::string ToHex(const std::vector<UCHAR>& digest) {
    static const char digits[] = "0123456789abcdef";
    std::string text;
    for (UCHAR byte : digest) {
        text.push_back(digits[byte >> 4]);
        text.push_back(digits[byte & 0x0F]);
    }
    return text;
}

}  // namespace

Reading<std::string> Sha256HexOfFile(const wchar_t* path) {
    using Result = Reading<std::string>;
    UniqueHandle file(CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file.valid()) {
        return Result::FromError(GetLastError());
    }
    AlgorithmHandle algorithm;
    HashHandle hash;
    if (algorithm.Open() != 0 || hash.Create(algorithm.get()) != 0) {
        return Result::Failure(Status::Error, ERROR_INVALID_DATA);
    }
    std::vector<UCHAR> buffer(64 * 1024);
    unsigned long error = 0;
    Step step = Step::More;
    do {
        step = HashStep(file.get(), hash.get(), buffer, error);
    } while (step == Step::More);
    if (step == Step::Failed) {
        return Result::Failure(Status::Error, error);
    }
    std::vector<UCHAR> digest(32);
    NTSTATUS finished = BCryptFinishHash(hash.get(), digest.data(), static_cast<ULONG>(digest.size()), 0);
    if (finished != 0) {
        return Result::Failure(Status::Error, ERROR_INVALID_DATA);
    }
    return Result::Success(ToHex(digest));
}

}  // namespace ews
