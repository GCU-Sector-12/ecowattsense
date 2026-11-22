// intel_dtt.cpp: Intel DTT presence through the service control manager (see ews/intel_dtt.h).
// Used by ews-scan. Only connect and query-status rights are requested, so a standard user can run it.

#include <windows.h>

#include "ews/intel_dtt.h"

namespace ews {

namespace {

/// Owns an SC_HANDLE (service manager or service) and closes it in the destructor.
class ServiceHandle {
public:
    explicit ServiceHandle(SC_HANDLE handle) noexcept : handle_(handle) {}
    ServiceHandle(const ServiceHandle&) = delete;
    ServiceHandle& operator=(const ServiceHandle&) = delete;
    ~ServiceHandle() {
        if (handle_ != nullptr) {
            CloseServiceHandle(handle_);
        }
    }

    /// @return True when the handle is usable.
    bool valid() const noexcept { return handle_ != nullptr; }
    /// @return The raw handle; this object keeps ownership.
    SC_HANDLE get() const noexcept { return handle_; }

private:
    SC_HANDLE handle_;
};

}  // namespace

Reading<bool> SystemIntelDttSource::IsPresent() {
    ServiceHandle manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!manager.valid()) {
        return Reading<bool>::FromError(GetLastError());
    }
    ServiceHandle service(OpenServiceW(manager.get(), L"esifsvc", SERVICE_QUERY_STATUS));
    if (service.valid()) {
        return Reading<bool>::Success(true);
    }
    DWORD error = GetLastError();
    // "Does not exist" is a real answer here; any other error (for example access denied) is unknown.
    if (error == ERROR_SERVICE_DOES_NOT_EXIST) {
        return Reading<bool>::Success(false);
    }
    return Reading<bool>::FromError(error);
}

}  // namespace ews
