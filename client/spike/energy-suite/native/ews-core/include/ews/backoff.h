// backoff.h: how long the service waits after failed sends (SPEC 5.5).
// Used by ews-service at each report tick. A pure rule, counted in report intervals.
#pragma once

namespace ews {

/// The number of report intervals to wait before the next try after `failuresInARow` failures.
/// The series is 1, 2, 4, 8, 16, 32, 60, 60, ... A success resets the count to 0, which gives 0 (try at the next tick).
/// @param failuresInARow Failed sends since the last success; a value below 1 means none.
/// @return 0 for no failure, otherwise 1, 2, 4, 8, 16, 32 and then 60.
int BackoffIntervals(int failuresInARow);

}  // namespace ews
