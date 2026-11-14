// event_queue.h: the small thread-safe queue between the service control handler and the main thread.
// Used by scm_host.cpp (the control handler pushes) and service_runtime.cpp (the main thread drains).
// The control handler runs on a thread of the service control manager and must return fast, so it only puts an
// event in the queue and signals; the main thread does the work.
#pragma once

#include <windows.h>

#include <mutex>
#include <vector>

#include "ews/handles.h"

namespace ews_service {

/// The events the main thread handles.
enum class ServiceEvent {
    Stop,          ///< Stop or shutdown.
    PowerSource,   ///< Mains or battery changed.
    Suspend,       ///< The computer goes to sleep.
    Resume,        ///< The computer woke up.
    DisplayOn,     ///< The console display went on.
    DisplayOff,    ///< The console display went off or dim.
    SessionChange,  ///< A session logged on or off, locked or unlocked, connected or disconnected.
    NetworkBack     ///< Windows reports that the internet is reachable (SPEC 5.5).
};

/// A queue of events with a signal the main thread can wait on.
class EventQueue {
public:
    EventQueue();

    /// @return The event handle that is signalled while the queue may hold events (for the wait set).
    HANDLE signal() const { return signal_.get(); }

    /// Adds an event and signals. Safe to call from any thread.
    /// @param event The event.
    void Push(ServiceEvent event);

    /// Takes every queued event, oldest first, and clears the signal. Called by the main thread only.
    /// @return The events (possibly none: the signal may have been set for an event taken before).
    std::vector<ServiceEvent> Drain();

private:
    ews::UniqueHandle signal_;
    std::mutex lock_;
    std::vector<ServiceEvent> events_;
};

}  // namespace ews_service
