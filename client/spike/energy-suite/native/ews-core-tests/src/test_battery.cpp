// test_battery.cpp: tests of the pure battery helper MilliwattHoursToMicrojoules.
#include <cstdint>

#include "ews/battery.h"
#include "test_common.h"

using ews::MilliwattHoursToMicrojoules;
using ews::Status;

EWS_TEST(MilliwattHoursNormal) {
    auto r = MilliwattHoursToMicrojoules(50000);  // 50 Wh
    CHECK(r.status == Status::Ok);
    CHECK(r.value == 180000000000ULL);
}

EWS_TEST(MilliwattHoursZero) {
    auto r = MilliwattHoursToMicrojoules(0);
    CHECK(r.status == Status::Ok);
    CHECK(r.value == 0);
}

EWS_TEST(MilliwattHoursLargestThatFits) {
    std::uint64_t largest = UINT64_MAX / 3600000ULL;
    auto r = MilliwattHoursToMicrojoules(largest);
    CHECK(r.status == Status::Ok);
    CHECK(r.value == largest * 3600000ULL);
}

EWS_TEST(MilliwattHoursOverflow) {
    std::uint64_t tooBig = UINT64_MAX / 3600000ULL + 1;
    auto r = MilliwattHoursToMicrojoules(tooBig);
    CHECK(r.status == Status::Error);
    CHECK(r.error != 0);
}
