// atomic_file.cpp: write to a temporary file, then rename it over the target (see ews/atomic_file.h).
// Used by ews-scan and, later, ews-service.
#include "ews/atomic_file.h"

#include <windows.h>

#include <filesystem>
#include <limits>

#include "ews/handles.h"

namespace ews {

namespace {

/// Writes the bytes to a new file and flushes them to disk. An existing file at the path is replaced.
/// @return Ok with true, or the failure with the Windows error code.
Reading<bool> WriteWholeFile(const std::filesystem::path& path, const std::string& bytes) {
    using Result = Reading<bool>;
    if (bytes.size() > static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())) {
        return Result::Failure(Status::Error, ERROR_FILE_TOO_LARGE);
    }
    UniqueHandle file(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file.valid()) {
        return Result::FromError(GetLastError());
    }
    DWORD written = 0;
    if (!WriteFile(file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr)) {
        return Result::FromError(GetLastError());
    }
    if (written != bytes.size()) {
        return Result::Failure(Status::Error, ERROR_WRITE_FAULT);
    }
    if (!FlushFileBuffers(file.get())) {
        return Result::FromError(GetLastError());
    }
    return Result::Success(true);
}

/// Renames `from` over `to`. @return Ok with true, or the failure (a sharing violation when `to` is open elsewhere).
Reading<bool> ReplaceFile(const std::filesystem::path& from, const std::filesystem::path& to) {
    if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return Reading<bool>::FromError(GetLastError());
    }
    return Reading<bool>::Success(true);
}

}  // namespace

Reading<bool> WriteFileAtomically(const std::wstring& folder, const std::wstring& name, const std::string& bytes) {
    std::filesystem::path target = std::filesystem::path(folder) / name;
    std::filesystem::path temporary = target;
    temporary += L".tmp";
    auto written = WriteWholeFile(temporary, bytes);
    if (!written.ok()) {
        DeleteFileW(temporary.c_str());
        return written;
    }
    auto replaced = ReplaceFile(temporary, target);
    if (!replaced.ok()) {
        DeleteFileW(temporary.c_str());
    }
    return replaced;
}

}  // namespace ews
