// test_backoff.cpp: tests of the back-off series (1, 2, 4, 8, 16, 32, 60, 60, ...).
#include "ews/backoff.h"
#include "test_common.h"

EWS_TEST(BackoffSeries) {
    CHECK(ews::BackoffIntervals(0) == 0);
    CHECK(ews::BackoffIntervals(1) == 1);
    CHECK(ews::BackoffIntervals(2) == 2);
    CHECK(ews::BackoffIntervals(3) == 4);
    CHECK(ews::BackoffIntervals(4) == 8);
    CHECK(ews::BackoffIntervals(5) == 16);
    CHECK(ews::BackoffIntervals(6) == 32);
    CHECK(ews::BackoffIntervals(7) == 60);
    CHECK(ews::BackoffIntervals(8) == 60);
    CHECK(ews::BackoffIntervals(100) == 60);
}

EWS_TEST(BackoffHandlesOddInput) {
    CHECK(ews::BackoffIntervals(-1) == 0);
    CHECK(ews::BackoffIntervals(-100000) == 0);
    CHECK(ews::BackoffIntervals(2147483647) == 60);
}
