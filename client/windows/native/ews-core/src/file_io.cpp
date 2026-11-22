// file_io.cpp: small file helpers (see ews/file_io.h).
// Used by the file parts of ews-core.
#include "ews/file_io.h"

#include <windows.h>

#include "ews/handles.h"

namespace ews {

namespace {

/// Opens a file for reading with full sharing. @return The handle (invalid on failure; read GetLastError()).
UniqueHandle OpenForRead(const std::filesystem::path& path) {
    return UniqueHandle(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
}

/// Reads exactly `size` bytes from an open file. @return Ok with the bytes, or the failure.
Reading<std::string> ReadExactly(const UniqueHandle& file, DWORD size) {
    std::string bytes(size, '\0');
    DWORD read = 0;
    if (!ReadFile(file.get(), bytes.data(), size, &read, nullptr)) {
        return Reading<std::string>::FromError(GetLastError());
    }
    if (read != size) {
        return Reading<std::string>::Failure(Status::Error, ERROR_READ_FAULT);
    }
    return Reading<std::string>::Success(std::move(bytes));
}

/// Turns the error of a failed open into a failure. A missing folder counts as a missing file (Unavailable).
Reading<std::string> OpenFailure(DWORD error) {
    if (error == ERROR_PATH_NOT_FOUND) {
        return Reading<std::string>::Failure(Status::Unavailable, error);
    }
    return Reading<std::string>::FromError(error);
}

}  // namespace

Reading<std::string> ReadWholeFile(const std::filesystem::path& path, std::size_t maxBytes) {
    using Result = Reading<std::string>;
    UniqueHandle file = OpenForRead(path);
    if (!file.valid()) {
        return OpenFailure(GetLastError());
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file.get(), &size)) {
        return Result::FromError(GetLastError());
    }
    if (static_cast<unsigned long long>(size.QuadPart) > maxBytes) {
        return Result::Failure(Status::Error, ERROR_FILE_TOO_LARGE);
    }
    return ReadExactly(file, static_cast<DWORD>(size.QuadPart));
}

Reading<bool> EnsureSubfolder(const std::wstring& dataDir, const std::wstring& name) {
    using Result = Reading<bool>;
    std::filesystem::path folder = std::filesystem::path(dataDir) / name;
    if (CreateDirectoryW(folder.c_str(), nullptr)) {
        return Result::Success(true);
    }
    DWORD error = GetLastError();
    if (error != ERROR_ALREADY_EXISTS) {
        return Result::FromError(error);
    }
    // The name may be taken by a file; only a real folder counts.
    DWORD attributes = GetFileAttributesW(folder.c_str());
    bool isFolder = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    return isFolder ? Result::Success(true) : Result::Failure(Status::Error, ERROR_DIRECTORY);
}

Reading<bool> ReplaceByRename(const std::filesystem::path& from, const std::filesystem::path& to) {
    if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return Reading<bool>::FromError(GetLastError());
    }
    return Reading<bool>::Success(true);
}

}  // namespace ews
