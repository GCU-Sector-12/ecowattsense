// battery.cpp: battery reader (see ews/battery.h).
#include "ews/battery.h"

#include <windows.h>

namespace ews {

Reading<std::vector<BatteryState>> SystemBatterySource::Read() {
    return Reading<std::vector<BatteryState>>::Failure(Status::Unavailable, 0);
}

Reading<std::uint64_t> MilliwattHoursToMicrojoules(std::uint64_t) {
    return Reading<std::uint64_t>::Success(1);
}

Reading<bool> ReadOnBattery() {
    return Reading<bool>::Failure(Status::Unavailable, 0);
}

}  // namespace ews
