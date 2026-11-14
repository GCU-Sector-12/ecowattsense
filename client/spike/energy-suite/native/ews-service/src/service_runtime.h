// service_runtime.h: the loop of ews-service around ServiceCore: the waitable timer, the pipe and the event queue.
// Used by console_host.cpp (console mode) and scm_host.cpp (service control manager mode), so both run the same loop.
// The main thread waits on four handles with WaitForMultipleObjects: the stop event, the timer, the pipe event and the
// signal of the event queue. Nothing else wakes it, so an idle service uses no processor time.
#pragma once

#include <windows.h>

#include <memory>

#include "event_queue.h"
#include "ews/service_args.h"
#include "ews/service_core.h"
#include "pipe_server.h"
#include "network_watch.h"

namespace ews_service {

/// The exit codes of the loop.
constexpr int kExitOk = 0;
constexpr int kExitStartFailed = 4;  ///< ServiceCore::Start refused (data folder, lock or identity).
constexpr int kExitTimerFailed = 5;  ///< The waitable timer could not be made.

/// Owns the real sources, the core, the pipe and the timer, and runs the loop until a stop.
class ServiceRuntime {
public:
    /// @param args The parsed command line (data folder, tick length, run limit).
    /// @param events The queue the service control handler fills (empty in console mode).
    /// @param stopEvent An event that, when signalled, stops the loop (Ctrl+C in console mode).
    ServiceRuntime(const ews::ServiceArgs& args, EventQueue& events, HANDLE stopEvent);

    /// Starts the core, creates the pipe and the timer, reads the session facts, runs the loop, then stops the core.
    /// @return kExitOk, or kExitStartFailed or kExitTimerFailed. A pipe that cannot be created is counted in the
    ///         log and does not stop the service.
    int Run();

private:
    bool MakeTimer();
    bool Step();
    void OnTimer();
    void OnPipe();
    void OnEvents();
    void HandleEvent(ServiceEvent event);
    void RefreshSessions();
    void LogCounted(long long code, unsigned long error);

    ews::ServiceArgs args_;
    EventQueue& events_;
    HANDLE stopEvent_;
    ews::SystemClock clock_;
    ews::SystemScanRunner scan_;
    ews::SystemEmiSource emi_;
    ews::SystemBatterySource battery_;
    ews::SystemCpuTimesSource cpu_;
    ews::SystemDisplaySource display_;
    ews::SystemPowerSource power_;
    ews::WinHttpSenderFactory senders_;
    ews::ServiceCore core_;
    std::unique_ptr<PipeServer> pipe_;
    std::unique_ptr<NetworkWatch> network_;
    ews::UniqueHandle timer_;
    bool done_ = false;
    int ticks_ = 0;
    unsigned long preferredSession_ = 0xFFFFFFFFul;
};

}  // namespace ews_service
