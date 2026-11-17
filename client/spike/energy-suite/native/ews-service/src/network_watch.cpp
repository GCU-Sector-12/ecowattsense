// network_watch.cpp: the registration for network changes (see network_watch.h).
// Used by service_runtime.cpp. The callbacks run on a Windows thread and must return fast: they only push an event.
#include <winsock2.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <netioapi.h>

#include "network_watch.h"

#pragma comment(lib, "iphlpapi.lib")

namespace ews_service {

namespace {

/// The level value of NetworkConnectivityLevelHintInternetAccess (4), kept as a number so that an older SDK still builds.
constexpr int kInternetAccessLevel = 4;

/// The part of NL_NETWORK_CONNECTIVITY_HINT this file reads. The real structure starts with the same enum.
struct ConnectivityHint {
    int connectivityLevel;
    int connectivityCost;
    BOOLEAN approachingDataLimit;
    BOOLEAN overDataLimit;
    BOOLEAN roaming;
};

using HintCallback = VOID(CALLBACK*)(PVOID context, ConnectivityHint hint);
using HintRegister = ULONG(WINAPI*)(HintCallback callback, PVOID context, BOOLEAN initialNotification, PHANDLE handle);

/// Called by Windows when the connectivity level changes: only "internet access" is reported to the loop.
VOID CALLBACK OnHint(PVOID context, ConnectivityHint hint) {
    if (hint.connectivityLevel != kInternetAccessLevel) {
        return;
    }
    static_cast<EventQueue*>(context)->Push(ServiceEvent::NetworkBack);
}

/// Called by Windows when an IP interface changes: only "an interface was added" is reported to the loop.
VOID CALLBACK OnInterface(PVOID context, PMIB_IPINTERFACE_ROW, MIB_NOTIFICATION_TYPE type) {
    if (type != MibAddInstance) {
        return;
    }
    static_cast<EventQueue*>(context)->Push(ServiceEvent::NetworkBack);
}

/// @return The hint registration function of iphlpapi.dll, or nullptr on a Windows without it.
HintRegister FindHintRegister() {
    HMODULE module = GetModuleHandleW(L"iphlpapi.dll");
    FARPROC found = module != nullptr ? GetProcAddress(module, "NotifyNetworkConnectivityHintChange") : nullptr;
    return reinterpret_cast<HintRegister>(found);
}

}  // namespace

NetworkWatch::NetworkWatch(EventQueue& events) {
    HintRegister registerHint = FindHintRegister();
    HANDLE handle = nullptr;
    bool hinted = registerHint != nullptr && registerHint(OnHint, &events, FALSE, &handle) == NO_ERROR;
    if (hinted) {
        handle_ = handle;
        return;
    }
    handle = nullptr;
    bool changed = NotifyIpInterfaceChange(AF_UNSPEC, OnInterface, &events, FALSE, &handle) == NO_ERROR;
    handle_ = changed ? handle : nullptr;
}

NetworkWatch::~NetworkWatch() {
    if (handle_ != nullptr) {
        CancelMibChangeNotify2(handle_);
    }
}

}  // namespace ews_service
