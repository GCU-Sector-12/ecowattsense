// backoff.h: how long the service waits after failed sends (SPEC 5.5).
// Used by ews-service at each report tick. A pure rule, counted in report intervals.
#pragma once

namespace ews {

/// The longest wait after failed sends, in seconds (30 minutes).
inline constexpr int kMaxBackoffSeconds = 30 * 60;

/// @param intervalSeconds The report interval in seconds; a value below 1 is taken as 1.
/// @return The cap of the wait in report intervals: 30 minutes divided by the interval, at least 1.
int BackoffCapIntervals(int intervalSeconds);

/// The number of report intervals to wait before the next try after `failuresInARow` failures.
/// The series is 1, 2, 4, 8, ... and never goes above the cap. A success resets the count to 0, which gives 0
/// (try at the next tick).
/// @param failuresInARow Failed sends since the last success; a value below 1 means none.
/// @param maxIntervals The cap in intervals (see BackoffCapIntervals); a value below 1 is taken as 1.
/// @return 0 for no failure, otherwise the next number of the series, at most `maxIntervals`.
int BackoffIntervals(int failuresInARow, int maxIntervals);

}  // namespace ews