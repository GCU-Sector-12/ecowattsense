# Run with PowerShell 7 (.NET 10): pwsh -File ./tests/Test-DonutGeometry.ps1
# Compiles the real HistoryRow and DonutGeometry (no WPF) and checks the shares, arc angles, empty and one-state cases,
# the rounding to 360 degrees, the 24 hour window, and that the colours are those of the state band of the 24 hour chart.
$ErrorActionPreference = 'Stop'
$gui = Join-Path $PSScriptRoot '../../EcoWattSense.Client.Gui/History'
Add-Type -Path @((Join-Path $gui 'HistoryRow.cs'), (Join-Path $gui 'DonutGeometry.cs'))
$checks = 0
function Check($ok, $what) { if (!$ok) { throw "Failed: $what" }; $script:checks++ }
function Near($a, $b) { [math]::Abs($a - $b) -lt 1e-9 }
function Utc($text) { [datetime]::SpecifyKind([datetime]::Parse($text, [cultureinfo]::InvariantCulture, 'AdjustToUniversal,AssumeUniversal'), 'Utc') }
function Row($end, $watts, $state, $seconds = 3600) {
    [EcoWattSense.Client.Gui.History.HistoryRow]::new((Utc $end), $seconds, $false, $state, $null, $null, $watts, $null, $null, 'L2')
}
function Build($rows, $now) { [EcoWattSense.Client.Gui.History.DonutGeometry]::Build([EcoWattSense.Client.Gui.History.HistoryRow[]]$rows, (Utc $now)) }
$now = '2026-11-14T12:00:00Z'

# Shares: Active 1 kWh, Idle 0.5 kWh, Unused 0.5 kWh. Order of the slices: Active, Idle, Unused.
$d = Build @((Row '2026-11-14T10:00:00Z' 1000 2), (Row '2026-11-14T09:00:00Z' 500 1), (Row '2026-11-14T08:00:00Z' 500 0)) $now
Check $d.HasData 'has data'
Check (Near $d.TotalKwh 2.0) 'total 2 kWh'
Check ($d.Slices.Count -eq 3) 'three slices'
Check (($d.Slices | ForEach-Object State) -join ',' -eq '2,1,0') 'slice order Active, Idle, Unused'
Check ((Near $d.Slices[0].Percent 50) -and (Near $d.Slices[1].Percent 25) -and (Near $d.Slices[2].Percent 25)) 'percent 50, 25, 25'
Check ((Near $d.Slices[0].SweepAngle 180) -and (Near $d.Slices[1].SweepAngle 90) -and (Near $d.Slices[2].SweepAngle 90)) 'sweeps 180, 90, 90'
Check ((Near $d.Slices[0].StartAngle 0) -and (Near $d.Slices[1].StartAngle 180) -and (Near $d.Slices[2].StartAngle 270)) 'starts 0, 180, 270'
Check ((Near $d.Slices[1].Kwh 0.5) -and (Near $d.Slices[2].Kwh 0.5)) 'kWh per state'

# No rows: empty donut.
$d = Build @() $now
Check (-not $d.HasData -and $d.TotalKwh -eq 0 -and $d.Slices.Count -eq 3) 'no rows'
Check (($d.Slices | Where-Object { $_.SweepAngle -ne 0 }).Count -eq 0) 'no rows: no arcs'

# Rows with an empty device W or an empty state are left out; so are negative watts and unknown states.
$d = Build @((Row '2026-11-14T10:00:00Z' $null 2), (Row '2026-11-14T10:00:00Z' 500 $null), (Row '2026-11-14T10:00:00Z' -5 2), (Row '2026-11-14T10:00:00Z' 500 7)) $now
Check (-not $d.HasData) 'only unusable rows'
$d = Build @((Row '2026-11-14T10:00:00Z' $null 2), (Row '2026-11-14T10:00:00Z' 1000 1)) $now
Check ($d.HasData -and (Near $d.TotalKwh 1.0)) 'unusable row left out, the other counted'
# Zero watts everywhere is no energy, so no data.
Check (-not (Build @((Row '2026-11-14T10:00:00Z' 0 2)) $now).HasData) 'zero watts'

# One state only: one full ring.
$d = Build @((Row '2026-11-14T10:00:00Z' 100 1)) $now
Check ((Near $d.Slices[1].StartAngle 0) -and (Near $d.Slices[1].SweepAngle 360) -and (Near $d.Slices[1].Percent 100)) 'one state: Idle is the full ring'
Check ($d.Slices[0].SweepAngle -eq 0 -and $d.Slices[2].SweepAngle -eq 0) 'one state: others empty'
$d = Build @((Row '2026-11-14T10:00:00Z' 100 0)) $now
Check (Near $d.Slices[2].SweepAngle 360) 'one state: Unused alone'

# Rounding: thirds do not add up exactly in floating point, but the arcs must close the ring at 360.
$d = Build @((Row '2026-11-14T10:00:00Z' 100 2 60), (Row '2026-11-14T10:00:00Z' 100 1 60), (Row '2026-11-14T10:00:00Z' 100 0 60)) $now
$sum = ($d.Slices | Measure-Object SweepAngle -Sum).Sum
Check (Near $sum 360) "thirds add up to 360 (got $sum)"
Check (Near ($d.Slices[2].StartAngle + $d.Slices[2].SweepAngle) 360) 'last arc ends at 360'
# Many odd values.
$random = [System.Random]::new(7)
for ($i = 0; $i -lt 200; $i++) {
    $rows = @(); foreach ($state in 2, 1, 0) { $rows += Row '2026-11-14T10:00:00Z' ($random.NextDouble() * 97.3 + 0.01) $state ($random.Next(1, 900)) }
    $s = (Build $rows $now).Slices
    Check (Near ($s | Measure-Object SweepAngle -Sum).Sum 360) "random set $i adds up to 360"
}
# A gap between arcs would show as a start that differs from the previous end.
Check ((Near $d.Slices[1].StartAngle ($d.Slices[0].StartAngle + $d.Slices[0].SweepAngle)) -and (Near $d.Slices[2].StartAngle ($d.Slices[1].StartAngle + $d.Slices[1].SweepAngle))) 'no gap between arcs'

# Window: a row that ended 24 hours ago (or earlier) is left out; one second newer counts.
Check (-not (Build @((Row '2026-11-13T12:00:00Z' 100 2)) $now).HasData) 'row exactly 24 h old left out'
Check ((Build @((Row '2026-11-13T12:00:01Z' 100 2)) $now).HasData) 'row 23 h 59 min 59 s old counted'
Check (-not (Build @((Row '2026-11-12T12:00:00Z' 100 2)) $now).HasData) 'row 48 h old left out'

# Points on the circle: 0 degrees is the top, 90 degrees is the right.
$top = [EcoWattSense.Client.Gui.History.DonutGeometry]::PointOnCircle(100, 100, 50, 0)
$right = [EcoWattSense.Client.Gui.History.DonutGeometry]::PointOnCircle(100, 100, 50, 90)
Check ((Near $top.Item1 100) -and (Near $top.Item2 50)) 'point at 0 degrees'
Check ((Near $right.Item1 150) -and (Near $right.Item2 100)) 'point at 90 degrees'

# Colours are exactly those of the state band of the 24 hour chart.
$chart = Get-Content -Raw (Join-Path $gui 'HistoryChart.cs')
foreach ($pair in @(@('ActiveBrush', [EcoWattSense.Client.Gui.History.StateColours]::Active), @('IdleBrush', [EcoWattSense.Client.Gui.History.StateColours]::Idle), @('UnusedBrush', [EcoWattSense.Client.Gui.History.StateColours]::Unused))) {
    Check ($chart -match ($pair[0] + ' = Make\("' + [regex]::Escape($pair[1]) + '"\)')) "$($pair[0]) colour $($pair[1]) is the one in HistoryChart.cs"
}
Write-Output "Passed $checks donut geometry checks."
