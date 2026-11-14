// pipe_server.h: the session pipe of SPEC section 6, `\\.\pipe\EcoWattSense.Service.Session`.
// Used by service_runtime.cpp. The pipe is inbound only, in message mode and overlapped, so the main thread never
// blocks on it: one event goes into the wait set, and OnSignal moves the state forward (listening, reading, done).
// Every byte from the pipe is untrusted. The pipe only reads; the core checks the text.
#pragma once

#include <windows.h>

#include <cstdint>
#include <optional>
#include <string>

#include "ews/handles.h"

namespace ews_service {

/// One message read from a client.
struct PipeInput {
    std::string text;                  ///< The bytes (at most 129: a longer message is cut, and the core refuses it).
    bool senderSessionActive = false;  ///< The session of the client is in the state active.
    unsigned long senderSession = 0;   ///< The session id of the client.
};

/// The pipe server. One instance, one client at a time.
class PipeServer {
public:
    /// Creates the pipe. When it fails (for example the name is taken) created() is false and error() says why.
    PipeServer();
    ~PipeServer();
    PipeServer(const PipeServer&) = delete;
    PipeServer& operator=(const PipeServer&) = delete;

    /// @return True when the pipe exists and listens.
    bool created() const { return pipe_.valid(); }

    /// @return The Windows error code of a failed creation, 0 when it worked.
    unsigned long error() const { return error_; }

    /// @return The event for the wait set. It is never null; without a pipe it is never signalled.
    HANDLE event() const { return event_.get(); }

    /// Moves the state forward after the event was signalled: a client connected (start reading), or a message
    /// arrived (return it, disconnect, listen again), or the read failed (disconnect, listen again).
    /// @return The message when one was completed, else nothing.
    std::optional<PipeInput> OnSignal();

    /// Called at every tick of the loop: a client that connected and sends nothing for 5 seconds is cut off, so one
    /// idle client cannot hold the pipe. Uses the tick, not a timer of its own.
    void OnTick();

private:
    enum class State { Idle, Listening, Reading };

    void BeginListening();
    void BeginReading();
    std::optional<PipeInput> CompleteRead();
    void Restart();

    ews::UniqueHandle event_;
    ews::UniqueHandle pipe_;
    OVERLAPPED overlapped_{};
    char buffer_[129] = {};
    State state_ = State::Idle;
    ULONGLONG readingSince_ = 0;
    unsigned long error_ = 0;
};

}  // namespace ews_service
