// cpu_times.cpp: GetSystemTimes reader and the processor load calculation (see ews/cpu_times.h).
// Used by ews-scan and ews-service.
#include "ews/cpu_times.h"

#include <windows.h>

namespace ews {

namespace {

/// Joins the two halves of a FILETIME into one 64-bit tick count.
/// @param time A FILETIME filled by GetSystemTimes.
/// @return The 100 ns ticks.
std::uint64_t ToTicks(const FILETIME& time) {
    return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}

}  // namespace

Reading<CpuTimes> SystemCpuTimesSource::Read() {
    FILETIME idle{};
    FILETIME kernel{};
    FILETIME user{};
    if (!GetSystemTimes(&idle, &kernel, &user)) {
        return Reading<CpuTimes>::FromError(GetLastError());
    }
    CpuTimes times;
    times.idle = ToTicks(idle);
    times.kernel = ToTicks(kernel);
    times.user = ToTicks(user);
    return Reading<CpuTimes>::Success(times);
}

Reading<double> CpuPercent(const CpuTimes& previous, const CpuTimes& current) {
    bool wentBackwards = current.idle < previous.idle || current.kernel < previous.kernel || current.user < previous.user;
    if (wentBackwards) {
        return Reading<double>::Failure(Status::Unavailable, 0);
    }
    std::uint64_t idle = current.idle - previous.idle;
    std::uint64_t kernel = current.kernel - previous.kernel;
    std::uint64_t user = current.user - previous.user;
    std::uint64_t total = kernel + user;  // kernel already contains the idle time
    bool overflowed = total < kernel;
    if (overflowed || total == 0 || idle > total) {
        return Reading<double>::Failure(Status::Unavailable, 0);
    }
    double busy = static_cast<double>(total - idle);
    return Reading<double>::Success(100.0 * busy / static_cast<double>(total));
}

}  // namespace ews
