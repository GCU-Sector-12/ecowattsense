# Windows report model

`DeviceReport` is an immutable outbound model for exactly the ten required fields
in `shared/report_schema.json`. Use `ReportJson.Serialize(report)` for the wire JSON.
It uses built-in System.Text.Json and does not send reports or collect measurements.

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
