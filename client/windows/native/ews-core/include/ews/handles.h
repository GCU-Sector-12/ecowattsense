// handles.h: small move-only owners for Windows handles, so a handle is always closed.
// Used by the readers in ews-core (battery, device enumeration).
#pragma once

#include <windows.h>
#include <setupapi.h>

namespace ews {

/// Owns a file or device HANDLE and closes it in the destructor.
/// Both INVALID_HANDLE_VALUE and nullptr count as "empty". Cannot be copied.
class UniqueHandle {
public:
    UniqueHandle() noexcept = default;
    /// Takes ownership of a handle.
    /// @param handle A handle from CreateFile and similar calls; may be invalid.
    explicit UniqueHandle(HANDLE handle) noexcept : handle_(handle) {}
    UniqueHandle(UniqueHandle&& other) noexcept : handle_(other.release()) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept;
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    ~UniqueHandle() { Reset(); }

    /// @return True when a usable handle is held.
    bool valid() const noexcept { return handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr; }
    /// @return The raw handle; the owner keeps ownership.
    HANDLE get() const noexcept { return handle_; }
    /// Gives up ownership without closing. @return The raw handle.
    HANDLE release() noexcept;
    /// Closes the held handle (if any) and takes a new one.
    /// @param handle The new handle, or the default for "empty".
    void Reset(HANDLE handle = INVALID_HANDLE_VALUE) noexcept;

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

/// Owns an HDEVINFO (SetupAPI device information set) and destroys it in the destructor.
/// Cannot be copied.
class UniqueDevInfo {
public:
    UniqueDevInfo() noexcept = default;
    /// Takes ownership of a set.
    /// @param set A value from SetupDiGetClassDevs; may be INVALID_HANDLE_VALUE.
    explicit UniqueDevInfo(HDEVINFO set) noexcept : set_(set) {}
    UniqueDevInfo(UniqueDevInfo&& other) noexcept : set_(other.release()) {}
    UniqueDevInfo& operator=(UniqueDevInfo&& other) noexcept;
    UniqueDevInfo(const UniqueDevInfo&) = delete;
    UniqueDevInfo& operator=(const UniqueDevInfo&) = delete;
    ~UniqueDevInfo() { Reset(); }

    /// @return True when a usable set is held.
    bool valid() const noexcept { return set_ != INVALID_HANDLE_VALUE && set_ != nullptr; }
    /// @return The raw set; the owner keeps ownership.
    HDEVINFO get() const noexcept { return set_; }
    /// Gives up ownership without destroying. @return The raw set.
    HDEVINFO release() noexcept;
    /// Destroys the held set (if any) and takes a new one.
    /// @param set The new set, or the default for "empty".
    void Reset(HDEVINFO set = INVALID_HANDLE_VALUE) noexcept;

private:
    HDEVINFO set_ = INVALID_HANDLE_VALUE;
};

}  // namespace ews
