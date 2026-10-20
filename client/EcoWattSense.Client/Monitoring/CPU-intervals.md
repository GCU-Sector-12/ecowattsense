# CPU interval samples

`CpuMonitor.GetSample()` takes one GetSystemTimes measurement and returns a status
plus an immutable sample when valid. BusyTime and TotalTime are CPU counter deltas
in 100-nanosecond ticks summed across processors, not wall-clock durations.
`GetUsagePercentage()` delegates to that call and returns the sample percentage,
or null as before. Call only one API per observation. If both are needed, use the
sample's UsagePercentage; do not call GetUsagePercentage again.

`CpuIntervalAccumulator` calculates `100 * sum(BusyTime) / sum(TotalTime)`.
It neither samples nor schedules reports. Empty and invalid intervals return null.
IsComplete means no gap has been reported; an empty interval still has no average.

Caller protocol (now used by Worker through ReportInterval):

1. At startup, prime the monitor. BaselineEstablished contains no elapsed sample.
   Start the first interval at that baseline boundary with a fresh/reset accumulator;
   do not include time before the baseline or add its null sample as elapsed time.
2. Add each subsequent Valid sample exactly once. Invalid readings, skipped/lost
   measurements, or API exceptions require MarkMissing (Add(null) has the same effect),
   even when no valid sample has arrived yet. Later valid samples cannot heal the interval.
3. An API failure clears the monitor baseline, as before. The next successful call
   returns BaselineEstablished. This does not repair an interval that already lost data.
   Explicitly discard that incomplete interval and Reset at this fresh baseline boundary
   to begin recovery. Alternatively wait for a valid sample, discard the incomplete
   interval including that sample, and Reset at its ending boundary.
4. Counter regression, zero elapsed time, and inconsistent deltas produce Invalid.
   The monitor retains the latest counters as its next baseline, as before. Conservatively
   resume at the ending boundary of a subsequent valid sample after discarding the
   incomplete interval and resetting; do not carry that recovery sample into the new interval.
5. For normal consecutive intervals: add the closing sample, read the average, then
   Reset at that sample's ending boundary. The monitor baseline stays intact. The next
   sample belongs entirely to the next interval.

Reset is caller-authorized abandonment/completion, not automatic gap recovery. Calling
it after a gap without an established counter boundary can incorrectly claim coverage.
Accumulator boundaries MUST align with CPU sample boundaries. Never split a sample,
reuse it across intervals, or reset halfway through its span. The accumulator cannot
detect silently omitted or duplicate samples; callers must signal gaps and enforce ownership.
Long intervals can be represented by larger deltas, but a skipped required observation
is conservatively treated as a gap under this protocol. Overflow of accumulated totals
also invalidates the interval until reset.

Worker uses GetSample once per observation and logs that sample's percentage.
ReportInterval schedules local reports at sample boundaries. Classification, identity,
and serialization retain their existing semantics.
