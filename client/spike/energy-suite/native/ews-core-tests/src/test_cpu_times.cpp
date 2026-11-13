// test_cpu_times.cpp: tests of CpuPercent (pure function over two samples).
#include "ews/cpu_times.h"
#include "test_common.h"

using ews::CpuPercent;
using ews::CpuTimes;
using ews::Status;

EWS_TEST(CpuPercentNormal) {
    CpuTimes before{0, 0, 0};
    CpuTimes after{600, 800, 200};  // total 1000, idle 600, busy 400
    auto r = CpuPercent(before, after);
    CHECK(r.status == Status::Ok);
    CHECK(r.value > 39.999 && r.value < 40.001);
}

EWS_TEST(CpuPercentAllIdle) {
    CpuTimes before{100, 150, 50};
    CpuTimes after{1100, 1150, 50};  // kernel grew by exactly the idle growth
    auto r = CpuPercent(before, after);
    CHECK(r.status == Status::Ok);
    CHECK(r.value >= 0.0 && r.value < 0.001);
}

EWS_TEST(CpuPercentFullyBusy) {
    CpuTimes before{0, 0, 0};
    CpuTimes after{0, 500, 500};
    auto r = CpuPercent(before, after);
    CHECK(r.status == Status::Ok);
    CHECK(r.value > 99.999 && r.value <= 100.0);
}

EWS_TEST(CpuPercentZeroDelta) {
    CpuTimes same{10, 20, 30};
    auto r = CpuPercent(same, same);
    CHECK(r.status == Status::Unavailable);
}

EWS_TEST(CpuPercentBackwardsCounter) {
    CpuTimes before{1000, 2000, 500};
    CpuTimes after{1100, 1900, 600};  // kernel went backwards
    auto r = CpuPercent(before, after);
    CHECK(r.status == Status::Unavailable);
}

EWS_TEST(CpuPercentIdleLargerThanTotal) {
    CpuTimes before{0, 0, 0};
    CpuTimes after{900, 500, 100};  // idle 900 > total 600
    auto r = CpuPercent(before, after);
    CHECK(r.status == Status::Unavailable);
}
