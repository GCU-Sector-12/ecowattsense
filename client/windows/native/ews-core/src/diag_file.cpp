// diag_file.cpp: the file writer of the diagnostics file (see ews/diag_file.h).
// Used by ews-service. Each line opens the file, appends and closes it, so no handle is held over a change of day.
#include "ews/diag_file.h"

#include <windows.h>

#include <filesystem>
#include <utility>

#include "ews/file_io.h"
#include "ews/handles.h"

namespace ews {

namespace {

/// Writes all bytes to an open file in one call. @return True when every byte was written.
bool WriteAll(const UniqueHandle& file, const std::string& bytes) {
    DWORD written = 0;
    bool done = WriteFile(file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) != 0;
    return done && written == bytes.size();
}

/// Opens the file of one day for appending. Others may read it. FILE_FLAG_WRITE_THROUGH sends each line to the disk.
/// @return The handle; not valid when the file cannot be opened.
UniqueHandle OpenForAppend(const std::filesystem::path& path) {
    return UniqueHandle(CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr));
}

/// @return True when the file holds so much that the new bytes would pass the limit. A size that cannot be read counts as not full.
bool WouldPassTheLimit(const UniqueHandle& file, std::size_t newBytes) {
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file.get(), &size)) {
        return false;
    }
    return static_cast<unsigned long long>(size.QuadPart) + newBytes > kMaxDiagFileBytes;
}

/// Deletes one directory entry when it is a plain file with an old diagnostics file name.
void DeleteIfOld(const std::filesystem::directory_entry& entry, UtcTime now) {
    std::error_code ignored;
    bool old = DiagFileIsOld(entry.path().filename().wstring(), now);
    bool plain = old && std::filesystem::symlink_status(entry.path(), ignored).type() == std::filesystem::file_type::regular;
    if (plain) {
        static_cast<void>(std::filesystem::remove(entry.path(), ignored));
    }
}

}  // namespace

DiagFileWriter::DiagFileWriter(std::wstring dataDir) : dataDir_(std::move(dataDir)) {}

DiagWriteResult DiagFileWriter::Append(UtcTime time, const std::string& line) {
    if (!EnsureSubfolder(dataDir_, L"logs").ok()) {
        return DiagWriteResult::Failed;
    }
    std::filesystem::path path = std::filesystem::path(dataDir_) / L"logs" / DiagFileName(time);
    UniqueHandle file = OpenForAppend(path);
    if (!file.valid()) {
        return DiagWriteResult::Failed;
    }
    std::string bytes = line + "\n";
    if (WouldPassTheLimit(file, bytes.size())) {
        return DiagWriteResult::Full;
    }
    return WriteAll(file, bytes) ? DiagWriteResult::Written : DiagWriteResult::Failed;
}

void DiagFileWriter::RemoveOld(UtcTime now) {
    std::error_code error;
    std::filesystem::directory_iterator iterator(std::filesystem::path(dataDir_) / L"logs", error);
    for (; !error && iterator != std::filesystem::directory_iterator(); iterator.increment(error)) {
        DeleteIfOld(*iterator, now);
    }
}

}  // namespace ews
