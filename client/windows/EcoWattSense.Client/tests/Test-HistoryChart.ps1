# Run with PowerShell 7 (.NET 10): pwsh -File ./tests/Test-HistoryChart.ps1
# Compiles the real HistoryRow and ChartGeometry (no WPF, no window) and checks the segments and rectangles.
$ErrorActionPreference = 'Stop'
$gui = Join-Path $PSScriptRoot '../../EcoWattSense.Client.Gui/History'
Add-Type -Path @((Join-Path $gui 'HistoryRow.cs'), (Join-Path $gui 'ChartGeometry.cs'))
$checks = 0
function Check($ok, $what) { if (!$ok) { throw "Failed: $what" }; $script:checks++ }
$now = [datetime]::new(2026, 11, 14, 10, 0, 30, [DateTimeKind]::Utc)
# Slot 1439 is the minute of $now (10:00); slot n ends at 10:00 minus (1439 - n) minutes.
function Slot($n) { [datetime]::new(2026, 11, 14, 10, 0, 0, [DateTimeKind]::Utc).AddMinutes($n - 1439).AddSeconds(5) }
function Row($n, $watts, $battery = $false, $state = 2) {
    [EcoWattSense.Client.Gui.History.HistoryRow]::new((Slot $n), 60, $battery, $state, $null, $null, $watts, $null, $null, 'L2')
}
function Build($rows, $zone = [TimeZoneInfo]::Utc) {
    [EcoWattSense.Client.Gui.History.ChartGeometry]::Build([EcoWattSense.Client.Gui.History.HistoryRow[]]$rows, $now, $zone)
}

# No rows: empty layout, default axis.
$l = Build @()
Check ($l.Segments.Count -eq 0 -and $l.BatteryRects.Count -eq 0 -and $l.StateRects.Count -eq 0 -and $l.AxisMax -eq 10) 'no rows'

# Line: slots 10,11,12 then a missing slot 13, then 14,15; an empty value at 16; a lone minute at 18.
$l = Build @((Row 10 5.0), (Row 11 6.0), (Row 12 7.0), (Row 14 4.0), (Row 15 4.5), (Row 16 $null), (Row 18 3.0))
Check ($l.Segments.Count -eq 3) "segments after gap and empty value: $($l.Segments.Count)"
Check ($l.Segments[0].Count -eq 3 -and $l.Segments[0][0].X -eq 10.5 -and $l.Segments[0][2].Y -eq 7.0) 'first segment'
Check ($l.Segments[1].Count -eq 2 -and $l.Segments[1][0].X -eq 14.5) 'segment after the gap'
Check ($l.Segments[2].Count -eq 2 -and $l.Segments[2][0].X -eq 18.0 -and $l.Segments[2][1].X -eq 19.0) 'lone minute is widened'

# Battery rectangles: runs of minutes on battery.
$l = Build @((Row 20 1 $true), (Row 21 1 $true), (Row 22 1 $false), (Row 23 1 $true), (Row 25 1 $true))
Check ($l.BatteryRects.Count -eq 3) "battery rectangles: $($l.BatteryRects.Count)"
Check ($l.BatteryRects[0].X -eq 20 -and $l.BatteryRects[0].Width -eq 2) 'battery run of two'
Check ($l.BatteryRects[1].X -eq 23 -and $l.BatteryRects[1].Width -eq 1 -and $l.BatteryRects[2].X -eq 25) 'battery singles'

# State rectangles: neighbouring equal states join; a gap or a null state breaks; run at the window end.
$l = Build @((Row 30 1 $false 2), (Row 31 1 $false 2), (Row 32 1 $false 1), (Row 33 1 $false $null), (Row 34 1 $false 0), (Row 1438 1 $false 2), (Row 1439 1 $false 2))
Check ($l.StateRects.Count -eq 4) "state rectangles: $($l.StateRects.Count)"
Check ($l.StateRects[0].State -eq 2 -and $l.StateRects[0].X -eq 30 -and $l.StateRects[0].Width -eq 2) 'active run'
Check ($l.StateRects[1].State -eq 1 -and $l.StateRects[1].Width -eq 1) 'idle run'
Check ($l.StateRects[2].State -eq 0 -and $l.StateRects[2].X -eq 34) 'unused run after a null state'
Check ($l.StateRects[3].X -eq 1438 -and $l.StateRects[3].Width -eq 2) 'run at the end of the window'

# Rows outside the window are dropped.
$old = [EcoWattSense.Client.Gui.History.HistoryRow]::new($now.AddHours(-25), 60, $false, 2, $null, $null, 99.0, $null, $null, 'L2')
$l = Build @($old)
Check ($l.Segments.Count -eq 0) 'row older than the window'

# Axis maximum: 1, 2, 5, 10 times a power of ten; ticks 0, half, max.
$g = [EcoWattSense.Client.Gui.History.ChartGeometry]
foreach ($case in @(@(0.3, 0.5), @(1, 1), @(1.2, 2), @(4.7, 5), @(5.1, 10), @(6.527, 10), @(10, 10), @(37, 50), @(180, 200), @(0, 10), @(-4, 10))) {
    Check ($g::NiceMax($case[0]) -eq $case[1]) "axis max for $($case[0])"
}
$l = Build @((Row 5 6.527))
Check ($l.AxisMax -eq 10 -and $l.WattTicks.Count -eq 3 -and $l.WattTicks[0] -eq 0 -and $l.WattTicks[1] -eq 5 -and $l.WattTicks[2] -eq 10) 'axis ticks'

# Hour labels every 3 hours of local time. Window: 2026-11-13 10:01 to 2026-11-14 10:00 UTC.
$l = Build @() ([TimeZoneInfo]::Utc)
$labels = ($l.HourTicks | ForEach-Object { $_.Label }) -join ' '
Check ($labels -eq '12:00 15:00 18:00 21:00 00:00 03:00 06:00 09:00') "UTC labels: $labels"
Check ($l.HourTicks[0].X -eq 119) 'first label position'
$plus2 = [TimeZoneInfo]::CreateCustomTimeZone('test+2', [TimeSpan]::FromHours(2), 'test+2', 'test+2')
$l = Build @() $plus2
$labels = ($l.HourTicks | ForEach-Object { $_.Label }) -join ' '
Check ($labels -eq '15:00 18:00 21:00 00:00 03:00 06:00 09:00 12:00') "UTC+2 labels: $labels"
$plus530 = [TimeZoneInfo]::CreateCustomTimeZone('test+5:30', [TimeSpan]::new(5, 30, 0), 'test+5:30', 'test+5:30')
$l = Build @() $plus530
$labels = ($l.HourTicks | ForEach-Object { $_.Label }) -join ' '
Check ($labels -eq '18:00 21:00 00:00 03:00 06:00 09:00 12:00 15:00') "UTC+5:30 labels: $labels"
# Measured part only: a dashed piece for rows with an empty device W and a measured W; the axis includes it.
function MRow($n, $device, $measured) {
    [EcoWattSense.Client.Gui.History.HistoryRow]::new((Slot $n), 60, $false, 2, $null, $null, $device, $measured, $null, $null)
}
$l = Build @((MRow 10 5.0 $null), (MRow 11 $null 3.0), (MRow 12 $null 4.0), (MRow 13 6.0 2.0), (MRow 14 $null $null))
Check ($l.Segments.Count -eq 2) 'device pieces'
Check ($l.MeasuredSegments.Count -eq 1 -and $l.MeasuredSegments[0].Count -eq 2 -and $l.MeasuredSegments[0][1].Y -eq 4.0) 'measured piece only where device W is empty'
$l = Build @((MRow 11 $null 30.0))
Check ($l.AxisMax -eq 50) 'axis covers measured values'
$l = Build @((Row 10 5.0))
Check ($l.MeasuredSegments.Count -eq 0) 'no measured piece without such rows'

Write-Output "Passed $checks history chart checks."
