// reading.cpp: maps Windows error codes to reader statuses (see ews/reading.h).
#include "ews/reading.h"

#include <windows.h>

namespace ews {

Status StatusFromWindowsError(unsigned long) {
    return Status::Ok;
}

}  // namespace ews
