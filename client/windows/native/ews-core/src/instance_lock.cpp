// instance_lock.cpp: the single-instance lock file (see ews/instance_lock.h).
// Used by ews-service. The same file name and sharing rule as SingleInstanceLock.cs of the client.
#include "ews/instance_lock.h"

#include <windows.h>

#include <filesystem>

#include "ews/file_io.h"

namespace ews {

LockState InstanceLock::Acquire(const std::wstring& dataDir) {
    error_ = 0;
    Reading<bool> folder = EnsureSubfolder(dataDir, L"Client");
    if (!folder.ok()) {
        error_ = folder.error;
        return LockState::Failed;
    }
    std::filesystem::path path = std::filesystem::path(dataDir) / L"Client" / L"instance.lock";
    // Read access only (a lock file made by another user is read-only for us); share mode 0 locks everybody else out.
    file_.Reset(CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (file_.valid()) {
        return LockState::Taken;
    }
    error_ = GetLastError();
    return error_ == ERROR_SHARING_VIOLATION ? LockState::Held : LockState::Failed;
}

void InstanceLock::Release() { file_.Reset(); }

}  // namespace ews
