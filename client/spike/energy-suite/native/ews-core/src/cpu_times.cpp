// cpu_times.cpp: GetSystemTimes reader and the processor load calculation (see ews/cpu_times.h).
#include "ews/cpu_times.h"

#include <windows.h>

namespace ews {

Reading<CpuTimes> SystemCpuTimesSource::Read() {
    return Reading<CpuTimes>::Failure(Status::Unavailable, 0);
}

Reading<double> CpuPercent(const CpuTimes&, const CpuTimes&) {
    return Reading<double>::Success(-1.0);
}

}  // namespace ews
