// winhttp_handle.h: a small move-free owner of a WinHTTP handle (session, connection or request), so it is always closed.
// Used by download.cpp of the web installer.
#pragma once

#include <windows.h>
#include <winhttp.h>

namespace websetup {

/// Owns one HINTERNET from WinHttpOpen, WinHttpConnect or WinHttpOpenRequest and closes it in the destructor.
/// Cannot be copied or moved.
class WinHttpHandle {
public:
    /// Takes ownership of a handle.
    /// @param handle A WinHTTP handle; may be null.
    explicit WinHttpHandle(HINTERNET handle) noexcept : handle_(handle) {}
    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;
    ~WinHttpHandle() {
        if (handle_ != nullptr) {
            WinHttpCloseHandle(handle_);
        }
    }

    /// @return True when a handle is held.
    bool valid() const noexcept { return handle_ != nullptr; }
    /// @return The raw handle; the owner keeps ownership.
    HINTERNET get() const noexcept { return handle_; }

private:
    HINTERNET handle_;
};

}  // namespace websetup
