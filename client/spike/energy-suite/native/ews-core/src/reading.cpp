// reading.cpp: maps Windows error codes to reader statuses (see ews/reading.h).
// Used by every reader through Reading<T>::FromError.
#include "ews/reading.h"

#include <windows.h>

namespace ews {

Status StatusFromWindowsError(unsigned long error) {
    switch (error) {
        case ERROR_ACCESS_DENIED:
            return Status::Denied;
        case ERROR_FILE_NOT_FOUND:
        case ERROR_NOT_FOUND:
        case ERROR_NO_MORE_ITEMS:
            return Status::Unavailable;
        default:
            return Status::Error;
    }
}

}  // namespace ews
