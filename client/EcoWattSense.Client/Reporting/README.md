# Windows report model

`DeviceReport` is an immutable outbound model for exactly the ten required fields
in `shared/report_schema.json`. Use `ReportJson.Serialize(report)` for the wire JSON.
It uses built-in System.Text.Json. Worker now supplies measurements to ReportInterval
and logs completed JSON. No reports are sent or buffered; skips and shutdown discard data.

Every constructor argument is required. If any measurement or classification is
unknown, the caller must wait or otherwise handle that condition, not replace it
with zero, false, or another invented value. `os` is always `windows`.

`cpuPercent` must be the average CPU utilisation over `intervalSeconds`, supplied
by the caller. This model does not calculate that average; a single monitor sample
is not automatically an average for the report interval. Durations are whole seconds;
this model does not round the activity monitor's fractional inactivity measurements.

`timestamp` is the end of the report interval. UTC DateTime values are retained;
Local values are converted using their local timezone. Unspecified DateTime values
are rejected. For an explicitly offset DateTimeOffset, pass its `UtcDateTime`.
Serialization preserves fractional seconds when present and always ends in `Z`.

Device types must be supplied explicitly and match the schema's lowercase ASCII
identifier pattern with a maximum length of 40. They are not restricted to laptop
and desktop. No device-type inference or measurement changes are performed.

The API is serialization-only; inbound JSON deserialization is not part of this task.
Optional energy fields and machine_hash are deliberately absent.

Run focused checks from the client directory:

```powershell
pwsh -NoProfile -File ./tests/Test-ReportJson.ps1
```

## Local live report generation

Set Reporting:DeviceType explicitly (there is no default). Reporting:IntervalSeconds
defaults to 300 and must be a positive integer. Both are validated before identity creation.
The five-second loop captures a Stopwatch-based boundary immediately after its single CPU
sample, plus a UTC timestamp for the interval end. The first CPU baseline starts timing.
Intervals close at the first observed boundary at or after the requested duration, including
the closing sample. Actual monotonic elapsed seconds and inactivity are floored with checked
integer conversions. UTC clock corrections do not change scheduling.

State is provisionally the state at the end of the interval, not a dominant state or a
time-weighted classification. Lock and power readings also come from the closing observation.
Unknown closing readings skip the report and start a new interval at that same boundary.
CPU failures/invalid samples invalidate coverage immediately. Recovery discards the old
interval and starts at a fresh baseline or the end of the next valid sample (which is discarded,
not reused). Sampling delays are represented by whole counter deltas; no catch-up reports or
sample splitting occur. See Monitoring/CPU-intervals.md for the ownership protocol.

From the repository root, a short Windows manual test with the existing test identity:

```powershell
dotnet run --project .\client\EcoWattSense.Client -- --Reporting:IntervalSeconds=15 --Reporting:DeviceType=laptop --DeviceIdentity:Directory="$env:TEMP\EcoWattSense-ManualIdentityTest"
```

Expect five-second measurement logs and a locally generated JSON report after approximately
15 seconds from the first baseline. Stop with Ctrl+C. The identity path is only a command-line
override; it is not saved in configuration. If Windows cannot supply a required reading,
expect an explicit skip reason instead of fabricated JSON.
