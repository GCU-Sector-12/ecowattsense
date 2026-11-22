// test_reading.cpp: tests of the Windows error to status mapping.
#include <windows.h>

#include "ews/reading.h"
#include "test_common.h"

using ews::Status;
using ews::StatusFromWindowsError;

EWS_TEST(StatusDenied) {
    CHECK(StatusFromWindowsError(ERROR_ACCESS_DENIED) == Status::Denied);
}

EWS_TEST(StatusUnavailable) {
    CHECK(StatusFromWindowsError(ERROR_FILE_NOT_FOUND) == Status::Unavailable);
    CHECK(StatusFromWindowsError(ERROR_NOT_FOUND) == Status::Unavailable);
    CHECK(StatusFromWindowsError(ERROR_NO_MORE_ITEMS) == Status::Unavailable);
}

EWS_TEST(StatusError) {
    CHECK(StatusFromWindowsError(ERROR_INVALID_PARAMETER) == Status::Error);
    CHECK(StatusFromWindowsError(ERROR_GEN_FAILURE) == Status::Error);
}

EWS_TEST(FromErrorKeepsCode) {
    auto r = ews::Reading<int>::FromError(ERROR_ACCESS_DENIED);
    CHECK(r.status == Status::Denied);
    CHECK(r.error == ERROR_ACCESS_DENIED);
    CHECK(!r.ok());
}
