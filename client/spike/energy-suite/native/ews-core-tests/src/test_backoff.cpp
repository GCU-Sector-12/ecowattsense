// test_backoff.cpp: tests of the back-off series (1, 2, 4, 8, ... up to the cap) and of the cap (30 minutes).
#include "ews/backoff.h"
#include "test_common.h"

EWS_TEST(BackoffSeriesUpToTheCap) {
    CHECK(ews::BackoffIntervals(0, 6) == 0);
    CHECK(ews::BackoffIntervals(1, 6) == 1);
    CHECK(ews::BackoffIntervals(2, 6) == 2);
    CHECK(ews::BackoffIntervals(3, 6) == 4);
    CHECK(ews::BackoffIntervals(4, 6) == 6);
    CHECK(ews::BackoffIntervals(5, 6) == 6);
    CHECK(ews::BackoffIntervals(100, 6) == 6);
    CHECK(ews::BackoffIntervals(7, 30) == 30);
    CHECK(ews::BackoffIntervals(5, 30) == 16);
    CHECK(ews::BackoffIntervals(3, 1) == 1);
}

EWS_TEST(BackoffHandlesOddInput) {
    CHECK(ews::BackoffIntervals(-1, 6) == 0);
    CHECK(ews::BackoffIntervals(-100000, 6) == 0);
    CHECK(ews::BackoffIntervals(2147483647, 6) == 6);
    CHECK(ews::BackoffIntervals(2147483647, 30) == 30);
    CHECK(ews::BackoffIntervals(3, 0) == 1);  // a cap below 1 is taken as 1
}

EWS_TEST(BackoffCapIsThirtyMinutes) {
    CHECK(ews::BackoffCapIntervals(60) == 30);
    CHECK(ews::BackoffCapIntervals(300) == 6);
    CHECK(ews::BackoffCapIntervals(1800) == 1);
    CHECK(ews::BackoffCapIntervals(3600) == 1);
    CHECK(ews::BackoffCapIntervals(0) == 1800);
    CHECK(ews::BackoffCapIntervals(-5) == 1800);
}
