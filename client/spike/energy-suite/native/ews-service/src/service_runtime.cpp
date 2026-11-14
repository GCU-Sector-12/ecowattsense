// service_runtime.cpp: the loop of ews-service (see service_runtime.h).
// Used by console_host.cpp and scm_host.cpp. One thread does all the work, so a session event that arrives during a
// tick waits in the queue until the tick is finished. One waitable timer, created without the resume flag, so it does
// not wake the computer from sleep.
#include "service_runtime.h"

#include <cstdio>

#include "ews/service_log.h"
#include "session_facts.h"

namespace ews_service {

namespace {

constexpr DWORD kHandleCount = 4;
constexpr long long kCountedPipeCreate = 10;  // CountedError code: the pipe could not be created.

/// @return The service settings for the core.
ews::ServiceSettings SettingsFrom(const ews::ServiceArgs& args) { return ews::ServiceSettings{args.dataDir, args.tickSeconds}; }

}  // namespace

ServiceRuntime::ServiceRuntime(const ews::ServiceArgs& args, EventQueue& events, HANDLE stopEvent)
    : args_(args),
      events_(events),
      stopEvent_(stopEvent),
      core_(ews::ServiceDeps{clock_, scan_, emi_, battery_, cpu_, display_, power_, senders_}, SettingsFrom(args)) {}

void ServiceRuntime::LogCounted(long long code, unsigned long error) {
    ews::AppendLog(args_.dataDir, clock_.Now(), ews::LogEvent::CountedError, {code, static_cast<long long>(error)});
}

bool ServiceRuntime::MakeTimer() {
    // Manual reset is false: the timer fires every tick and the wait clears the signal. No resume flag: sleep is not interrupted.
    timer_.Reset(CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS));
    if (!timer_.valid()) {
        return false;
    }
    LARGE_INTEGER due{};
    due.QuadPart = -static_cast<LONGLONG>(args_.tickSeconds) * 10000000LL;
    return SetWaitableTimer(timer_.get(), &due, args_.tickSeconds * 1000L, nullptr, nullptr, FALSE) != 0;
}

void ServiceRuntime::RefreshSessions() {
    SessionFacts facts = ReadSessionFacts(preferredSession_);
    core_.SessionsChanged(facts.somebodyLoggedOn, facts.activeSessionLocked);
}

int ServiceRuntime::Run() {
    ews::StartResult started = core_.Start();
    if (started != ews::StartResult::Ok) {
        std::fprintf(stderr, "ews-service: start failed (%s)\n", ews::StartResultWord(started));
        return kExitStartFailed;
    }
    pipe_ = std::make_unique<PipeServer>();
    if (!pipe_->created()) {
        LogCounted(kCountedPipeCreate, pipe_->error());
    }
    if (!MakeTimer()) {
        core_.Stop();
        return kExitTimerFailed;
    }
    RefreshSessions();
    while (!done_) {
        done_ = Step();
    }
    core_.Stop();
    return kExitOk;
}

bool ServiceRuntime::Step() {
    HANDLE handles[kHandleCount] = {stopEvent_, timer_.get(), pipe_->event(), events_.signal()};
    DWORD index = WaitForMultipleObjects(kHandleCount, handles, FALSE, INFINITE) - WAIT_OBJECT_0;
    switch (index) {
        case 0: return true;
        case 1: OnTimer(); return done_;
        case 2: OnPipe(); return done_;
        case 3: OnEvents(); return done_;
        default: return true;  // a failed wait: stop cleanly rather than spin
    }
}

void ServiceRuntime::OnTimer() {
    core_.Tick();
    pipe_->OnTick();
    ++ticks_;
    bool limited = args_.runSeconds > 0;
    done_ = done_ || (limited && ticks_ * args_.tickSeconds >= args_.runSeconds);
}

void ServiceRuntime::OnPipe() {
    std::optional<PipeInput> input = pipe_->OnSignal();
    if (!input.has_value()) {
        return;
    }
    core_.PipeMessage(input->text, input->senderSessionActive);
    bool newSender = input->senderSessionActive && input->senderSession != preferredSession_;
    preferredSession_ = newSender ? input->senderSession : preferredSession_;
    if (newSender) {
        RefreshSessions();
    }
}

void ServiceRuntime::OnEvents() {
    for (ServiceEvent event : events_.Drain()) {
        HandleEvent(event);
    }
}

void ServiceRuntime::HandleEvent(ServiceEvent event) {
    switch (event) {
        case ServiceEvent::Stop: done_ = true; return;
        case ServiceEvent::PowerSource: core_.PowerSourceChanged(); return;
        case ServiceEvent::Suspend: core_.Suspend(); return;
        case ServiceEvent::Resume: core_.Resume(); return;
        case ServiceEvent::DisplayOn: core_.DisplayChanged(true); return;
        case ServiceEvent::DisplayOff: core_.DisplayChanged(false); return;
        case ServiceEvent::SessionChange: RefreshSessions(); return;
    }
}

}  // namespace ews_service
