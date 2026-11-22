// scm_host.cpp: the service control manager mode of ews-service (SPEC 5.1 to 5.3).
// Used when ews-service.exe runs with no argument and the service control manager started it.
// The control handler runs on a thread of the manager and must return fast: it only puts an event in the queue.
// The service reports "running" first, before the slow start work (the scan takes seconds). This path needs a
// registered service and administrator rights to test; the console mode runs the same loop.
#include <windows.h>
#include <powrprof.h>

#include "ews/handles.h"
#include "hosts.h"
#include "service_runtime.h"

#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")

namespace ews_service {

namespace {

constexpr const wchar_t* kServiceName = L"EcoWattSenseService";
constexpr DWORD kStopWaitHintMs = 5000;

/// The state the control handler and ServiceMain share. The manager gives no context to ServiceMain, so one global points here.
struct ScmContext {
    ews::ServiceArgs args;
    EventQueue events;
    SERVICE_STATUS_HANDLE handle = nullptr;
};

ScmContext* g_context = nullptr;

/// Tells the manager the state of the service.
/// @param state SERVICE_RUNNING, SERVICE_STOP_PENDING or SERVICE_STOPPED.
/// @param exitCode 0 for a clean stop, else the program exit code (reported as a service specific code).
void ReportState(ScmContext& context, DWORD state, int exitCode) {
    SERVICE_STATUS status{};
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = state;
    bool running = state == SERVICE_RUNNING;
    status.dwControlsAccepted = running ? (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_POWEREVENT | SERVICE_ACCEPT_SESSIONCHANGE) : 0;
    status.dwWin32ExitCode = exitCode == 0 ? NO_ERROR : ERROR_SERVICE_SPECIFIC_ERROR;
    status.dwServiceSpecificExitCode = static_cast<DWORD>(exitCode);
    status.dwWaitHint = state == SERVICE_STOP_PENDING ? kStopWaitHintMs : 0;
    SetServiceStatus(context.handle, &status);
}

/// Turns a power setting change into an event. Only the power source and the console display state are of interest.
void OnPowerSetting(ScmContext& context, const POWERBROADCAST_SETTING& setting) {
    if (IsEqualGUID(setting.PowerSetting, GUID_ACDC_POWER_SOURCE)) {
        context.events.Push(ServiceEvent::PowerSource);
        return;
    }
    bool display = IsEqualGUID(setting.PowerSetting, GUID_CONSOLE_DISPLAY_STATE) && setting.DataLength >= sizeof(DWORD);
    if (!display) {
        return;
    }
    DWORD value = *reinterpret_cast<const DWORD*>(setting.Data);
    context.events.Push(value == 0 ? ServiceEvent::DisplayOff : ServiceEvent::DisplayOn);
}

/// Handles a power event of the manager.
void OnPowerEvent(ScmContext& context, DWORD eventType, LPVOID eventData) {
    switch (eventType) {
        case PBT_APMSUSPEND: context.events.Push(ServiceEvent::Suspend); return;
        case PBT_APMRESUMEAUTOMATIC: context.events.Push(ServiceEvent::Resume); return;
        case PBT_APMRESUMESUSPEND: context.events.Push(ServiceEvent::Resume); return;
        case PBT_APMPOWERSTATUSCHANGE: context.events.Push(ServiceEvent::PowerSource); return;
        case PBT_POWERSETTINGCHANGE: OnPowerSetting(context, *static_cast<const POWERBROADCAST_SETTING*>(eventData)); return;
        default: return;
    }
}

/// The control handler. Runs on a thread of the manager; it only reports and queues.
DWORD WINAPI ControlHandler(DWORD control, DWORD eventType, LPVOID eventData, LPVOID context) {
    ScmContext& state = *static_cast<ScmContext*>(context);
    switch (control) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            ReportState(state, SERVICE_STOP_PENDING, 0);
            state.events.Push(ServiceEvent::Stop);
            return NO_ERROR;
        case SERVICE_CONTROL_POWEREVENT:
            OnPowerEvent(state, eventType, eventData);
            return NO_ERROR;
        case SERVICE_CONTROL_SESSIONCHANGE:
            state.events.Push(ServiceEvent::SessionChange);
            return NO_ERROR;
        case SERVICE_CONTROL_INTERROGATE:
            return NO_ERROR;
        default:
            return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

/// Asks the manager for power setting events of one kind.
/// @return The registration, or null when it failed (the service then runs without that event).
HPOWERNOTIFY Register(ScmContext& context, const GUID& setting) {
    return RegisterPowerSettingNotification(context.handle, &setting, DEVICE_NOTIFY_SERVICE_HANDLE);
}

/// Runs the service after the handler is registered: reports running, runs the loop, reports stopped.
void RunService(ScmContext& context) {
    ReportState(context, SERVICE_RUNNING, 0);
    HPOWERNOTIFY powerSource = Register(context, GUID_ACDC_POWER_SOURCE);
    HPOWERNOTIFY displayState = Register(context, GUID_CONSOLE_DISPLAY_STATE);
    ews::UniqueHandle neverSet(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    int code = ServiceRuntime(context.args, context.events, neverSet.get()).Run();
    UnregisterPowerSettingNotification(powerSource);
    UnregisterPowerSettingNotification(displayState);
    ReportState(context, SERVICE_STOPPED, code);
}

/// The entry point the manager calls on its own thread.
void WINAPI ServiceMain(DWORD, LPWSTR*) {
    g_context->handle = RegisterServiceCtrlHandlerExW(kServiceName, ControlHandler, g_context);
    if (g_context->handle == nullptr) {
        return;
    }
    RunService(*g_context);
}

}  // namespace

int RunUnderScm(const ews::ServiceArgs& args) {
    ScmContext context;
    context.args = args;
    g_context = &context;
    SERVICE_TABLE_ENTRYW table[] = {{const_cast<LPWSTR>(kServiceName), ServiceMain}, {nullptr, nullptr}};
    bool started = StartServiceCtrlDispatcherW(table) != 0;
    g_context = nullptr;
    return started ? 0 : 1;
}

}  // namespace ews_service
