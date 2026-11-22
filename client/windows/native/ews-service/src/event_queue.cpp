// event_queue.cpp: the thread-safe event queue (see event_queue.h).
// Used by the service control handler and the main thread of ews-service.
#include "event_queue.h"

namespace ews_service {

EventQueue::EventQueue() : signal_(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {}

void EventQueue::Push(ServiceEvent event) {
    {
        std::lock_guard<std::mutex> guard(lock_);
        events_.push_back(event);
    }
    SetEvent(signal_.get());
}

std::vector<ServiceEvent> EventQueue::Drain() {
    // The signal is cleared before the queue is taken: an event pushed in between sets it again, so none is missed.
    ResetEvent(signal_.get());
    std::lock_guard<std::mutex> guard(lock_);
    std::vector<ServiceEvent> taken;
    taken.swap(events_);
    return taken;
}

}  // namespace ews_service
