// watch_run.cpp: the watch mode of ews-scan.exe (see watch_run.h).
// Used by main.cpp. Windows calls (Microsoft Learn): CreateWaitableTimerW, SetWaitableTimer (relative due time is negative,
// in 100 ns units; a positive period makes it periodic), WaitForMultipleObjects, CreateEventW, SetEvent, GetStdHandle,
// ReadFile (a closed pipe gives ERROR_BROKEN_PIPE; end of file gives zero bytes). The clock is std::chrono::steady_clock.
// A helper thread reads standard input and throws the bytes away; when the read ends it signals an event. That event and
// the timer are the only two things the main thread waits for.

#include <windows.h>
#include <fcntl.h>
#include <io.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#include "ews/battery.h"
#include "ews/display.h"
#include "ews/display_layout.h"
#include "ews/emi.h"
#include "ews/gpu.h"
#include "ews/gpu_use.h"
#include "ews/handles.h"
#include "ews/machine_info.h"
#include "ews/watch_session.h"
#include "watch_run.h"

namespace ews_scan {

namespace {

constexpr int kExitEnded = 0;
constexpr int kExitFailed = 1;
constexpr DWORD kInputBufferBytes = 256;
constexpr LONGLONG kHundredNanosecondsPerMillisecond = 10000;

/// Reads standard input until it ends, then signals the event. Runs on the helper thread.
/// The event is never closed: the system closes it when the process ends, so a late signal cannot hit a closed handle.
/// @param endEvent The event to signal.
void ReadUntilEnd(HANDLE endEvent) {
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    char buffer[kInputBufferBytes];
    DWORD got = 1;
    while (got > 0 && ReadFile(input, buffer, kInputBufferBytes, &got, nullptr)) {
    }
    SetEvent(endEvent);
}

/// Writes one line to standard output and flushes it.
/// @return True when the whole line was written.
bool WriteLine(const std::string& line) {
    bool written = std::fwrite(line.data(), 1, line.size(), stdout) == line.size();
    return written && std::fflush(stdout) == 0;
}

/// @return The milliseconds that passed since `start` on the monotonic clock.
std::int64_t ElapsedMs(std::chrono::steady_clock::time_point start) {
    auto passed = std::chrono::steady_clock::now() - start;
    return std::chrono::duration_cast<std::chrono::milliseconds>(passed).count();
}

/// Starts the periodic timer: the first signal comes after one period.
/// @return True when SetWaitableTimer succeeded.
bool StartTimer(HANDLE timer, std::uint32_t periodMs) {
    LARGE_INTEGER due;
    due.QuadPart = -static_cast<LONGLONG>(periodMs) * kHundredNanosecondsPerMillisecond;
    return SetWaitableTimer(timer, &due, static_cast<LONG>(periodMs), nullptr, nullptr, FALSE) != FALSE;
}

/// The real sources of this computer.
struct RealSources {
    ews::SystemMachineInfoSource machine;
    ews::SystemEmiSource emi;
    ews::SystemBatterySource battery;
    ews::SystemDisplaySource display;
    ews::SystemDisplayLayoutSource displayLayout;
    ews::SystemGpuSource gpu;
    ews::SystemNvmlSession nvml;
    ews::SystemGpuUseSource gpuUse;
};

/// Prints one line for every timer signal until the input ends or the output closes.
/// @return The result of the last wait.
DWORD Loop(ews::WatchSession& session, HANDLE timer, HANDLE endEvent, std::chrono::steady_clock::time_point start) {
    HANDLE waits[2] = {timer, endEvent};
    DWORD waited = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
    while (waited == WAIT_OBJECT_0 && WriteLine(session.Sample(ElapsedMs(start)))) {
        waited = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
    }
    return waited;
}

}  // namespace

int RunWatch(std::uint32_t periodMs) {
    _setmode(_fileno(stdout), _O_BINARY);  // no carriage returns: one JSON line ends with a line feed
    ews::UniqueHandle timer(CreateWaitableTimerW(nullptr, FALSE, nullptr));
    HANDLE endEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!timer.valid() || endEvent == nullptr) {
        return kExitFailed;
    }
    RealSources real;
    ews::WatchSources sources{real.machine, real.emi, real.battery, real.display, real.displayLayout,
                              real.gpu,     real.nvml, real.gpuUse,  ews::ReadOnBattery};
    ews::WatchSession session(sources, periodMs);
    if (!WriteLine(session.Start())) {
        return kExitEnded;
    }
    session.Sample(0);  // the first good read of every counter: the lines count from here
    std::thread(ReadUntilEnd, endEvent).detach();
    auto start = std::chrono::steady_clock::now();
    if (!StartTimer(timer.get(), periodMs)) {
        return kExitFailed;
    }
    DWORD last = Loop(session, timer.get(), endEvent, start);
    return last == WAIT_FAILED ? kExitFailed : kExitEnded;
}

}  // namespace ews_scan
