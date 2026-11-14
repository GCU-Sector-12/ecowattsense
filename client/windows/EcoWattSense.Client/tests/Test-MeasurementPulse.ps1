# Run with PowerShell 7 (.NET 10): pwsh -File ./tests/Test-MeasurementPulse.ps1
# Compiles the real HistoryRow and MeasurementInfo (no WPF) and checks the Last measurement line and the new-row trigger
# that makes the Service dot pulse: the same last row gives no pulse, a new last row gives one.
$ErrorActionPreference = 'Stop'
$root = Join-Path $PSScriptRoot '../../EcoWattSense.Client.Gui'
Add-Type -Path @((Join-Path $root 'History/HistoryRow.cs'), (Join-Path $root 'Viewer/MeasurementInfo.cs'))
$checks = 0
function Check($ok, $what) { if (!$ok) { throw "Failed: $what" }; $script:checks++ }
function Utc($text) { [datetime]::SpecifyKind([datetime]::Parse($text, [cultureinfo]::InvariantCulture, 'AdjustToUniversal,AssumeUniversal'), 'Utc') }
function Row($end) { [EcoWattSense.Client.Gui.History.HistoryRow]::new((Utc $end), 60, $false, 2, $null, $null, 5.0, $null, $null, 'L2') }
$info = [EcoWattSense.Client.Gui.Viewer.MeasurementInfo]
$plus2 = [TimeZoneInfo]::CreateCustomTimeZone('test+2', [TimeSpan]::FromHours(2), 'test+2', 'test+2')
$none = [Nullable[datetime]]$null

# Last end time: the newest of unsorted rows; null without rows.
$rows = [EcoWattSense.Client.Gui.History.HistoryRow[]]@((Row '2026-11-14T10:05:00Z'), (Row '2026-11-14T10:07:00Z'), (Row '2026-11-14T10:06:00Z'))
Check ($info::LastEndUtc($rows) -eq (Utc '2026-11-14T10:07:00Z')) 'newest row of unsorted input'
Check ($null -eq $info::LastEndUtc([EcoWattSense.Client.Gui.History.HistoryRow[]]@())) 'no rows'

# The line: local time as HH:mm; unknown without a row.
Check ($info::Line((Utc '2026-11-14T10:05:00Z'), $plus2) -eq 'Last measurement 12:05') 'line in UTC+2'
Check ($info::Line((Utc '2026-11-14T10:05:00Z'), [TimeZoneInfo]::Utc) -eq 'Last measurement 10:05') 'line in UTC'
Check ($info::Line((Utc '2026-11-14T23:59:00Z'), $plus2) -eq 'Last measurement 01:59') 'line after local midnight'
Check ($info::Line($none, $plus2) -eq 'Last measurement unknown') 'line without a row'

# The pulse trigger.
$tracker = [EcoWattSense.Client.Gui.Viewer.NewRowTracker]::new()
$t1 = Utc '2026-11-14T10:05:00Z'
$t2 = Utc '2026-11-14T10:06:00Z'
Check (-not $tracker.Observe($t1)) 'first read only records (opening the window is not a new row)'
Check (-not $tracker.Observe($t1)) 'same last row: no pulse'
Check (-not $tracker.Observe($t1)) 'same last row again: no pulse'
Check ($tracker.Observe($t2)) 'new last row: one pulse'
Check (-not $tracker.Observe($t2)) 'the new row seen again: no second pulse'
Check (-not $tracker.Observe($none)) 'empty read: no pulse'
Check (-not $tracker.Observe($t2)) 'after an empty read the same row is still not new'
Check (-not $tracker.Observe($t1)) 'an older row is not new'
Check ($tracker.Observe((Utc '2026-11-14T10:07:00Z'))) 'a later row is new again'
$fresh = [EcoWattSense.Client.Gui.Viewer.NewRowTracker]::new()
Check (-not $fresh.Observe($none)) 'empty first read: no pulse'
Check (-not $fresh.Observe($t1)) 'first row after an empty start only records'
Check ($fresh.Observe($t2)) 'then a new row pulses'
Write-Output "Passed $checks measurement pulse checks."
