// pipe_server.cpp: the session pipe (see pipe_server.h).
// Used by ews-service. The access rule is an SDDL string: SYSTEM and Administrators full access, interactive users
// write access. The mask 0x120196 is FILE_GENERIC_WRITE plus FILE_READ_ATTRIBUTES: CreateFile asks for the attribute
// right on a pipe (measured: without it the open fails with error 5). A generic right (GA, GW) in an explicit ACE of a
// pipe grants nothing, so file rights are written out. FILE_FLAG_FIRST_PIPE_INSTANCE makes the creation fail when
// another program already owns the name, so nobody can take the name before the service and read what clients send.
#include "pipe_server.h"

#include <sddl.h>

#include "session_facts.h"

#pragma comment(lib, "advapi32.lib")

namespace ews_service {

namespace {

constexpr const wchar_t* kPipeName = L"\\\\.\\pipe\\EcoWattSense.Service.Session";
constexpr const wchar_t* kPipeSddl = L"D:(A;;FA;;;SY)(A;;FA;;;BA)(A;;0x120196;;;IU)";
constexpr DWORD kReadTimeoutMs = 5000;

/// Frees the security descriptor that ConvertStringSecurityDescriptor... allocated.
struct LocalMemory {
    void* data = nullptr;
    ~LocalMemory() { LocalFree(data); }
};

/// Creates the pipe with the access rule. @return The handle, or an invalid handle (read GetLastError()).
HANDLE CreatePipeHandle() {
    LocalMemory descriptor;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(kPipeSddl, SDDL_REVISION_1, &descriptor.data, nullptr)) {
        return INVALID_HANDLE_VALUE;
    }
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), descriptor.data, FALSE};
    DWORD open = PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE;
    DWORD mode = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS;
    return CreateNamedPipeW(kPipeName, open, mode, 4, 0, 256, 0, &attributes);
}

}  // namespace

PipeServer::PipeServer() : event_(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {
    pipe_.Reset(CreatePipeHandle());
    error_ = pipe_.valid() ? 0 : GetLastError();
    if (pipe_.valid()) {
        BeginListening();
    }
}

PipeServer::~PipeServer() {
    if (pipe_.valid()) {
        CancelIoEx(pipe_.get(), nullptr);
    }
}

void PipeServer::BeginListening() {
    state_ = State::Listening;
    overlapped_ = OVERLAPPED{};
    overlapped_.hEvent = event_.get();
    ConnectNamedPipe(pipe_.get(), &overlapped_);
    DWORD error = GetLastError();
    // A client that connected before the call returns this error; the event is not set by the system then.
    if (error == ERROR_PIPE_CONNECTED) {
        SetEvent(event_.get());
    }
}

void PipeServer::BeginReading() {
    state_ = State::Reading;
    readingSince_ = GetTickCount64();
    overlapped_ = OVERLAPPED{};
    overlapped_.hEvent = event_.get();
    // The buffer holds one byte more than the longest valid message, so a message that is too long can be seen.
    ReadFile(pipe_.get(), buffer_, sizeof(buffer_), nullptr, &overlapped_);
}

void PipeServer::Restart() {
    DisconnectNamedPipe(pipe_.get());
    ResetEvent(event_.get());
    BeginListening();
}

std::optional<PipeInput> PipeServer::CompleteRead() {
    DWORD bytes = 0;
    BOOL done = GetOverlappedResult(pipe_.get(), &overlapped_, &bytes, FALSE);
    // ERROR_MORE_DATA: the message did not fit in 129 bytes. The cut text is passed on and the core refuses it.
    bool usable = done || GetLastError() == ERROR_MORE_DATA;
    if (!usable) {
        return std::nullopt;
    }
    PipeInput input;
    input.text.assign(buffer_, bytes);
    ULONG session = 0;
    bool known = GetNamedPipeClientSessionId(pipe_.get(), &session) != 0;
    input.senderSession = known ? session : 0;
    input.senderSessionActive = known && IsSessionActive(session);
    return input;
}

std::optional<PipeInput> PipeServer::OnSignal() {
    if (!pipe_.valid()) {
        return std::nullopt;
    }
    if (state_ == State::Listening) {
        BeginReading();
        return std::nullopt;
    }
    std::optional<PipeInput> input = CompleteRead();
    Restart();
    return input;
}

void PipeServer::OnTick() {
    bool stuck = pipe_.valid() && state_ == State::Reading && GetTickCount64() - readingSince_ > kReadTimeoutMs;
    if (stuck) {
        // The cancelled read signals the event; OnSignal then sees a failed read and listens again.
        CancelIoEx(pipe_.get(), &overlapped_);
    }
}

}  // namespace ews_service
