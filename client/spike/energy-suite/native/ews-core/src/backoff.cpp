// backoff.cpp: the back-off series after failed sends (see ews/backoff.h).
// Used by ews-service.
#include "ews/backoff.h"

#include <algorithm>

namespace ews {

namespace {

constexpr int kMaxWaitIntervals = 60;
constexpr int kLastDoublingFailure = 6;  // 6 failures give 2^5 = 32; the 7th and later give the cap.

}  // namespace

int BackoffIntervals(int failuresInARow) {
    int failures = std::max(0, failuresInARow);
    int capped = std::min(failures, kLastDoublingFailure + 1);
    bool atCap = capped > kLastDoublingFailure;
    int doubled = capped == 0 ? 0 : 1 << (capped - 1);
    return atCap ? kMaxWaitIntervals : doubled;
}

}  // namespace ews
