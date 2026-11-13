// machine_info.cpp: the real machine facts source (see ews/machine_info.h).
#include "ews/machine_info.h"

#include <windows.h>

namespace ews {

Reading<MachineInfo> SystemMachineInfoSource::Read() {
    return Reading<MachineInfo>::Failure(Status::Unavailable, 0);
}

}  // namespace ews
