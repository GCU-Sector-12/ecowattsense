// session_facts.h: the session facts the service needs (SPEC 6): is somebody logged on, and is the active session locked.
// Used by service_runtime.cpp at start and on every session event, and by pipe_server.cpp to check the sender.
// Windows Terminal Services calls only; the service never polls these, it asks when an event says something changed.
#pragma once

namespace ews_service {

/// The value for "no preferred session".
inline constexpr unsigned long kNoSession = 0xFFFFFFFFul;

/// The two facts the core takes.
struct SessionFacts {
    bool somebodyLoggedOn = false;     ///< At least one session is active and has a user name.
    bool activeSessionLocked = false;  ///< The chosen active session is locked.
};

/// Lists the sessions, finds the active ones that have a user name, and reads the lock flag of one of them:
/// the preferred session when it is among them, else the first. A flag that cannot be read counts as not locked.
/// @param preferredSession The session that sent the last accepted pipe message, or kNoSession.
/// @return The facts. A failed enumeration gives "nobody logged on".
SessionFacts ReadSessionFacts(unsigned long preferredSession);

/// Tells whether a session is in the state "active" (the console or a remote desktop session that is connected).
/// @param sessionId The session id.
/// @return True for the state active; false for any other state or when the query fails.
bool IsSessionActive(unsigned long sessionId);

}  // namespace ews_service
