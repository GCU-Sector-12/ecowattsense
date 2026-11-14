// console_host.cpp: the `--console` mode of ews-service (SPEC 5.2), the same loop as the service, for tests.
// Used by scripts/Test-ServiceConsole.ps1 and by hand. Ctrl+C, Ctrl+Break and closing the window stop it cleanly.
#include <windows.h>

#include "ews/handles.h"
#include "hosts.h"
#include "service_runtime.h"

namespace ews_service {

namespace {

/// The event the console handler signals. The handler runs on its own thread, so it can only use a global.
HANDLE g_stopEvent = nullptr;

/// Handles Ctrl+C, Ctrl+Break and the close of the console: signals the stop event.
/// @return TRUE for the three events (handled), FALSE for any other.
BOOL WINAPI OnConsoleControl(DWORD type) {
    bool stops = type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT;
    if (stops) {
        SetEvent(g_stopEvent);
    }
    return stops ? TRUE : FALSE;
}

}  // namespace

int RunConsole(const ews::ServiceArgs& args) {
    ews::UniqueHandle stop(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    g_stopEvent = stop.get();
    SetConsoleCtrlHandler(OnConsoleControl, TRUE);
    EventQueue events;
    int code = ServiceRuntime(args, events, stop.get()).Run();
    SetConsoleCtrlHandler(OnConsoleControl, FALSE);
    g_stopEvent = nullptr;
    return code;
}

}  // namespace ews_service
