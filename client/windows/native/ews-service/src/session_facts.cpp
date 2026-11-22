// session_facts.cpp: the session facts (see session_facts.h).
// Used by ews-service. Memory from the WTS calls is always freed by a small owner class.
#include "session_facts.h"

#include <windows.h>
#include <wtsapi32.h>

#include <algorithm>
#include <vector>

#pragma comment(lib, "wtsapi32.lib")

namespace ews_service {

namespace {

/// Owns memory returned by a WTS call and frees it.
class WtsMemory {
public:
    WtsMemory() = default;
    WtsMemory(const WtsMemory&) = delete;
    WtsMemory& operator=(const WtsMemory&) = delete;
    ~WtsMemory() { WTSFreeMemory(data_); }
    /// @return The address that the WTS call fills.
    void** address() { return &data_; }
    /// @return The memory, or null.
    void* get() const { return data_; }

private:
    void* data_ = nullptr;
};

/// Asks for one piece of session information.
/// @return True when the call worked and `memory` holds `bytes` bytes.
bool Query(unsigned long sessionId, WTS_INFO_CLASS info, WtsMemory& memory, DWORD& bytes) {
    return WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionId, info, reinterpret_cast<LPWSTR*>(memory.address()), &bytes) != 0;
}

/// @return True when the session has a user name (the text is not empty).
bool HasUserName(unsigned long sessionId) {
    WtsMemory memory;
    DWORD bytes = 0;
    if (!Query(sessionId, WTSUserName, memory, bytes)) {
        return false;
    }
    const wchar_t* name = static_cast<const wchar_t*>(memory.get());
    return name != nullptr && name[0] != L'\0';
}

/// Adds a session id to the list when it is active and has a user name.
void AddIfActiveWithUser(std::vector<unsigned long>& list, unsigned long sessionId) {
    if (IsSessionActive(sessionId) && HasUserName(sessionId)) {
        list.push_back(sessionId);
    }
}

/// @return The ids of the active sessions that have a user name.
std::vector<unsigned long> ActiveSessionsWithUser() {
    std::vector<unsigned long> found;
    WTS_SESSION_INFOW* sessions = nullptr;
    DWORD count = 0;
    if (!WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessions, &count)) {
        return found;
    }
    for (DWORD index = 0; index < count; ++index) {
        AddIfActiveWithUser(found, sessions[index].SessionId);
    }
    WTSFreeMemory(sessions);
    return found;
}

/// @return The preferred session when it is in the list, else the first one.
unsigned long ChooseSession(const std::vector<unsigned long>& list, unsigned long preferred) {
    bool listed = std::find(list.begin(), list.end(), preferred) != list.end();
    return listed ? preferred : list.front();
}

/// @return True when the session reports the lock state "locked". Anything else, also a failed query, is "not locked".
bool IsLocked(unsigned long sessionId) {
    WtsMemory memory;
    DWORD bytes = 0;
    if (!Query(sessionId, WTSSessionInfoEx, memory, bytes) || bytes < sizeof(WTSINFOEXW)) {
        return false;
    }
    const WTSINFOEXW* info = static_cast<const WTSINFOEXW*>(memory.get());
    bool levelOne = info->Level == 1;
    return levelOne && info->Data.WTSInfoExLevel1.SessionFlags == static_cast<LONG>(WTS_SESSIONSTATE_LOCK);
}

}  // namespace

bool IsSessionActive(unsigned long sessionId) {
    WtsMemory memory;
    DWORD bytes = 0;
    if (!Query(sessionId, WTSConnectState, memory, bytes) || bytes < sizeof(int)) {
        return false;
    }
    return *static_cast<const int*>(memory.get()) == static_cast<int>(WTSActive);
}

SessionFacts ReadSessionFacts(unsigned long preferredSession) {
    SessionFacts facts;
    std::vector<unsigned long> active = ActiveSessionsWithUser();
    if (active.empty()) {
        return facts;
    }
    facts.somebodyLoggedOn = true;
    facts.activeSessionLocked = IsLocked(ChooseSession(active, preferredSession));
    return facts;
}

}  // namespace ews_service
