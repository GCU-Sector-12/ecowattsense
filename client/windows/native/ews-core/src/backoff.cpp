// backoff.cpp: the back-off series after failed sends (see ews/backoff.h).
// Used by ews-service.
#include "ews/backoff.h"

#include <algorithm>

namespace ews {

namespace {

constexpr int kMaxDoublings = 20;  // 2^20 is far above any cap, and the shift stays inside an int.

}  // namespace

int BackoffCapIntervals(int intervalSeconds) { return std::max(1, kMaxBackoffSeconds / std::max(1, intervalSeconds)); }

int BackoffIntervals(int failuresInARow, int maxIntervals) {
    int failures = std::clamp(failuresInARow, 0, kMaxDoublings + 1);
    int doubled = failures == 0 ? 0 : 1 << (failures - 1);
    return std::min(doubled, std::max(1, maxIntervals));
}

}  // namespace ews