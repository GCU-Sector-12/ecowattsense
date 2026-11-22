// handles.cpp: implementation of the handle owners declared in ews/handles.h.
#include "ews/handles.h"

#include <utility>

namespace ews {

UniqueHandle& UniqueHandle::operator=(UniqueHandle&& other) noexcept {
    // Release first, so a self-move does not close the handle it keeps.
    HANDLE incoming = other.release();
    Reset(incoming);
    return *this;
}

HANDLE UniqueHandle::release() noexcept {
    HANDLE old = handle_;
    handle_ = INVALID_HANDLE_VALUE;
    return old;
}

void UniqueHandle::Reset(HANDLE handle) noexcept {
    HANDLE old = std::exchange(handle_, handle);
    if (old == INVALID_HANDLE_VALUE || old == nullptr) {
        return;
    }
    CloseHandle(old);
}

UniqueDevInfo& UniqueDevInfo::operator=(UniqueDevInfo&& other) noexcept {
    HDEVINFO incoming = other.release();
    Reset(incoming);
    return *this;
}

HDEVINFO UniqueDevInfo::release() noexcept {
    HDEVINFO old = set_;
    set_ = INVALID_HANDLE_VALUE;
    return old;
}

void UniqueDevInfo::Reset(HDEVINFO set) noexcept {
    HDEVINFO old = std::exchange(set_, set);
    if (old == INVALID_HANDLE_VALUE || old == nullptr) {
        return;
    }
    SetupDiDestroyDeviceInfoList(old);
}

}  // namespace ews
