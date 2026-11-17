// network_watch.h: tells the service loop when the internet becomes reachable (SPEC 5.5).
// Used by service_runtime.cpp. It registers with Windows for connectivity changes; Windows calls back on its own
// thread, and the callback only pushes ServiceEvent::NetworkBack into the event queue. The loop thread does the work,
// so there is no second sending thread and no polling.
#pragma once

#include <windows.h>

#include "event_queue.h"

namespace ews_service {

/// Owns one registration for network changes. The registration ends in the destructor.
class NetworkWatch {
public:
    /// Registers with NotifyNetworkConnectivityHintChange (Windows 10 2004 and later, looked up at run time). When that
    /// function is missing or fails, it falls back to NotifyIpInterfaceChange (an interface was added).
    /// @param events The queue that gets ServiceEvent::NetworkBack. It must outlive this object.
    explicit NetworkWatch(EventQueue& events);

    /// Ends the registration and waits for a running callback to finish.
    ~NetworkWatch();

    NetworkWatch(const NetworkWatch&) = delete;
    NetworkWatch& operator=(const NetworkWatch&) = delete;

    /// @return True when one of the two registrations worked. A false value is not fatal: the timer still retries.
    bool registered() const { return handle_ != nullptr; }

private:
    HANDLE handle_ = nullptr;
};

}  // namespace ews_service
