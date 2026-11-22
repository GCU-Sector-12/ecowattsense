# Run with PowerShell 7 (.NET 10): pwsh -File ./tests/Test-HistorySummary.ps1
# Compiles the real HistoryRow and HistorySummary (no WPF) and checks power now and energy today.
$ErrorActionPreference = 'Stop'
$gui = Join-Path $PSScriptRoot '../../EcoWattSense.Client.Gui/History'
Add-Type -Path @((Join-Path $gui 'HistoryRow.cs'), (Join-Path $gui 'HistorySummary.cs'))
$checks = 0
function Check($ok, $what) { if (!$ok) { throw "Failed: $what" }; $script:checks++ }
function Utc($text) { [datetime]::SpecifyKind([datetime]::Parse($text, [cultureinfo]::InvariantCulture, 'AdjustToUniversal,AssumeUniversal'), 'Utc') }
function Row($end, $watts, $seconds = 60) {
    [EcoWattSense.Client.Gui.History.HistoryRow]::new((Utc $end), $seconds, $false, 2, $null, $null, $watts, $null, $null, 'L2')
}
function Power($rows, $now) { [EcoWattSense.Client.Gui.History.HistorySummary]::PowerNow([EcoWattSense.Client.Gui.History.HistoryRow[]]$rows, (Utc $now)) }
function Energy($rows, $now, $zone) { [EcoWattSense.Client.Gui.History.HistorySummary]::EnergyTodayKwh([EcoWattSense.Client.Gui.History.HistoryRow[]]$rows, (Utc $now), $zone) }
$utc = [TimeZoneInfo]::Utc
$plus2 = [TimeZoneInfo]::CreateCustomTimeZone('test+2', [TimeSpan]::FromHours(2), 'test+2', 'test+2')

# Power now: fresh, stale, empty value, no rows, newest row chosen from unsorted input.
Check ((Power @((Row '2026-11-14T10:00:00Z' 7.5)) '2026-11-14T10:02:59Z') -eq 7.5) 'fresh row'
Check ($null -eq (Power @((Row '2026-11-14T10:00:00Z' 7.5)) '2026-11-14T10:03:00Z')) 'stale row (3 minutes)'
Check ($null -eq (Power @((Row '2026-11-14T10:00:00Z' $null)) '2026-11-14T10:01:00Z')) 'fresh row with empty value'
Check ($null -eq (Power @() '2026-11-14T10:01:00Z')) 'no rows'
Check ((Power @((Row '2026-11-14T10:00:00Z' 9), (Row '2026-11-14T09:00:00Z' 1)) '2026-11-14T10:01:00Z') -eq 9) 'newest row of unsorted input'

# Energy today: 1000 W for 3600 s = 1 kWh.
Check ([math]::Abs((Energy @((Row '2026-11-14T10:00:00Z' 1000 3600)) '2026-11-14T12:00:00Z' $utc) - 1.0) -lt 1e-9) 'one kWh'
Check ($null -eq (Energy @() '2026-11-14T12:00:00Z' $utc)) 'no rows'
Check ($null -eq (Energy @((Row '2026-11-14T10:00:00Z' $null)) '2026-11-14T12:00:00Z' $utc)) 'only empty values'

# Empty values are left out; yesterday's rows are left out.
$rows = @((Row '2026-11-14T10:00:00Z' 600 60), (Row '2026-11-14T10:01:00Z' $null 60), (Row '2026-11-13T23:59:00Z' 5000 60))
Check ([math]::Abs((Energy $rows '2026-11-14T12:00:00Z' $utc) - (600 * 60 / 3600000)) -lt 1e-12) 'empty and old rows left out'

# Midnight local time (UTC+2): the local day starts at 22:00 UTC.
$rows = @((Row '2026-11-13T21:59:00Z' 1000 60), (Row '2026-11-13T22:00:00Z' 1000 60), (Row '2026-11-14T05:00:00Z' 2000 60))
$expected = (1000 * 60 + 2000 * 60) / 3600000
Check ([math]::Abs((Energy $rows '2026-11-14T06:00:00Z' $plus2) - $expected) -lt 1e-12) 'local day UTC+2'
# The same rows seen in UTC: the first two rows are on 13 November, so only the last counts.
Check ([math]::Abs((Energy $rows '2026-11-14T06:00:00Z' $utc) - (2000 * 60 / 3600000)) -lt 1e-12) 'same rows in UTC'
# Late in the local evening the UTC date is still the same day but the local day has just ended.
Check ($null -eq (Energy @((Row '2026-11-14T10:00:00Z' 100)) '2026-11-14T22:30:00Z' $plus2)) 'new local day has no rows yet'
# Measured part only: a row with an empty device W but a measured W shows the measured value.
function MRow($end, $device, $measured, $seconds = 60) {
    [EcoWattSense.Client.Gui.History.HistoryRow]::new((Utc $end), $seconds, $false, 2, $null, $null, $device, $measured, $null, $null)
}
$summary = [EcoWattSense.Client.Gui.History.HistorySummary]
$reading = $summary::PowerNowReading([EcoWattSense.Client.Gui.History.HistoryRow[]]@((MRow '2026-11-14T10:00:00Z' $null 4.5)), (Utc '2026-11-14T10:01:00Z'))
Check ($reading.Watts -eq 4.5 -and $reading.MeasuredOnly) 'measured part shown when device W is empty'
$reading = $summary::PowerNowReading([EcoWattSense.Client.Gui.History.HistoryRow[]]@((MRow '2026-11-14T10:00:00Z' 7.5 4.5)), (Utc '2026-11-14T10:01:00Z'))
Check ($reading.Watts -eq 7.5 -and -not $reading.MeasuredOnly) 'device W wins over measured W'
$reading = $summary::PowerNowReading([EcoWattSense.Client.Gui.History.HistoryRow[]]@((MRow '2026-11-14T10:00:00Z' $null $null)), (Utc '2026-11-14T10:01:00Z'))
Check ($null -eq $reading.Watts -and -not $reading.MeasuredOnly) 'both empty is unknown'
$reading = $summary::PowerNowReading([EcoWattSense.Client.Gui.History.HistoryRow[]]@((MRow '2026-11-14T10:00:00Z' $null 4.5)), (Utc '2026-11-14T10:03:00Z'))
Check ($null -eq $reading.Watts) 'stale measured row is unknown'
# Today with mixed rows: 1000 W device for 1800 s plus 1000 W measured for 1800 s = 1 kWh, flagged.
$mixed = [EcoWattSense.Client.Gui.History.HistoryRow[]]@((MRow '2026-11-14T10:00:00Z' 1000 $null 1800), (MRow '2026-11-14T10:30:00Z' $null 1000 1800))
$today = $summary::EnergyToday($mixed, (Utc '2026-11-14T12:00:00Z'), $utc)
Check ([math]::Abs($today.Kwh - 1.0) -lt 1e-9 -and $today.IncludesMeasuredOnly) 'today from mixed rows'
$today = $summary::EnergyToday([EcoWattSense.Client.Gui.History.HistoryRow[]]@((MRow '2026-11-14T10:00:00Z' 1000 $null 3600)), (Utc '2026-11-14T12:00:00Z'), $utc)
Check (-not $today.IncludesMeasuredOnly) 'device rows only are not flagged'
$today = $summary::EnergyToday([EcoWattSense.Client.Gui.History.HistoryRow[]]@((MRow '2026-11-13T23:59:00Z' $null 1000 60), (MRow '2026-11-14T10:00:00Z' 1000 $null 3600)), (Utc '2026-11-14T12:00:00Z'), $utc)
Check (-not $today.IncludesMeasuredOnly) 'a measured row of yesterday is not flagged'

Write-Output "Passed $checks history summary checks."
