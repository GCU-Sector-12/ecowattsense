# PowerShell 7; synthetic counters only, no native Windows measurements.
$ErrorActionPreference = 'Stop'
Add-Type -Path @(
    (Join-Path $PSScriptRoot '../Monitoring/CpuSample.cs'),
    (Join-Path $PSScriptRoot '../Monitoring/CpuIntervalAccumulator.cs'),
    (Join-Path $PSScriptRoot '../Monitoring/CpuMonitor.cs')
)
$script:checks = 0
function Assert-Check([bool] $condition, [string] $message) {
    if (!$condition) { throw $message }
    $script:checks++
}
function Assert-Average($accumulator, [double] $expected) {
    $actual = $accumulator.GetAveragePercentage()
    Assert-Check ($null -ne $actual -and [Math]::Abs($actual - $expected) -lt 0.0000001) "Expected $expected, got $actual"
}
$accumulator = [EcoWattSense.Client.Monitoring.CpuIntervalAccumulator]::new()
Assert-Check ($null -eq $accumulator.GetAveragePercentage()) 'Empty interval fabricated an average.'
$low = [EcoWattSense.Client.Monitoring.CpuSample]::new(10, 100)
$high = [EcoWattSense.Client.Monitoring.CpuSample]::new(900, 900)
$accumulator.Add($low)
$accumulator.Add($high)
Assert-Average $accumulator 91
Assert-Check (($low.UsagePercentage + $high.UsagePercentage) / 2 -eq 55) 'Expected unweighted average to be 55, not 91.'

$accumulator.Reset()
Assert-Check ($null -eq $accumulator.GetAveragePercentage()) 'Reset retained a previous average.'
$accumulator.Add([EcoWattSense.Client.Monitoring.CpuSample]::new(0, 500))
Assert-Average $accumulator 0
$accumulator.Reset()
$accumulator.Add([EcoWattSense.Client.Monitoring.CpuSample]::new(500, 500))
Assert-Average $accumulator 100
$accumulator.Reset()
$accumulator.Add([EcoWattSense.Client.Monitoring.CpuSample]::new(25, 100))
Assert-Average $accumulator 25

$accumulator.Add($null)
Assert-Check (!$accumulator.IsComplete -and $null -eq $accumulator.GetAveragePercentage()) 'Missing sample did not invalidate interval.'
$accumulator.Add($high)
Assert-Check ($null -eq $accumulator.GetAveragePercentage()) 'Valid sample healed an incomplete interval.'
$accumulator.Reset()
$accumulator.MarkMissing()
$accumulator.Add($low)
Assert-Check ($null -eq $accumulator.GetAveragePercentage()) 'Gap before first valid sample was ignored.'
$accumulator.Reset()
$accumulator.Add($low)
Assert-Average $accumulator 10

foreach ($pair in @(@(0, 0), @(0, -1), @(-1, 10), @(11, 10), @([double]::NaN, 10),
    @(0, [double]::NaN), @([double]::PositiveInfinity, 10), @(0, [double]::PositiveInfinity),
    @([double]::NegativeInfinity, 10))) {
    $rejected = $false
    try { $null = [EcoWattSense.Client.Monitoring.CpuSample]::new($pair[0], $pair[1]) }
    catch {
        if ($_.Exception.GetBaseException() -isnot [ArgumentOutOfRangeException]) { throw }
        $rejected = $true
    }
    Assert-Check $rejected "Accepted invalid sample: $pair"
}
$accumulator.Reset()
$accumulator.Add([EcoWattSense.Client.Monitoring.CpuSample]::new([double]::MaxValue, [double]::MaxValue))
Assert-Average $accumulator 100
$accumulator.Add([EcoWattSense.Client.Monitoring.CpuSample]::new([double]::MaxValue, [double]::MaxValue))
Assert-Check ($null -eq $accumulator.GetAveragePercentage()) 'Sum overflow must invalidate interval.'
$accumulator.Reset()
$accumulator.Add($low)
Assert-Average $accumulator 10

# Exercise the actual monitor's counter processing without invoking GetSystemTimes.
$monitor = [EcoWattSense.Client.Monitoring.CpuMonitor]::new()
$process = $monitor.GetType().GetMethod('ProcessCounters', [Reflection.BindingFlags]'Instance,NonPublic')
function Read-Counters([ulong] $idle, [ulong] $kernel, [ulong] $user) {
    return $process.Invoke($monitor, [object[]]@($idle, $kernel, $user))
}
$reading = Read-Counters 100 200 300
Assert-Check ($reading.Status.ToString() -eq 'BaselineEstablished' -and $null -eq $reading.Sample) 'Initial reading must only establish baseline.'
$accumulator.Reset() # First interval starts here, not before the initial baseline.
Assert-Check ($null -eq $accumulator.GetAveragePercentage()) 'Baseline fabricated coverage.'
$reading = Read-Counters 130 250 350
Assert-Check ($reading.Status.ToString() -eq 'Valid') 'Expected valid delta.'
Assert-Check ($reading.Sample.BusyTime -eq 70 -and $reading.Sample.TotalTime -eq 100) 'Counter deltas incorrect.'
$legacyPercentage = 100.0 * (100 - 30) / 100
Assert-Check ($reading.Sample.UsagePercentage -eq $legacyPercentage) 'Percentage differs from existing calculation.'
$accumulator.Add($reading.Sample)
Assert-Average $accumulator 70

$reading = Read-Counters 130 250 350
Assert-Check ($reading.Status.ToString() -eq 'Invalid' -and $null -eq $reading.Sample) 'Zero elapsed counters must be invalid.'
$accumulator.Add($reading.Sample)
$reading = Read-Counters 10 20 30
Assert-Check ($reading.Status.ToString() -eq 'Invalid') 'Counter regression must be invalid, not initial baseline.'
$reading = Read-Counters 100 21 31
Assert-Check ($reading.Status.ToString() -eq 'Invalid') 'Idle greater than total must be invalid.'
$reading = Read-Counters 110 41 51
Assert-Check ($reading.Status.ToString() -eq 'Valid' -and $reading.Sample.UsagePercentage -eq 75) 'Monitor must recover using latest counters.'
$accumulator.Add($reading.Sample)
Assert-Check ($null -eq $accumulator.GetAveragePercentage()) 'Recovery must not repair old interval.'
$accumulator.Reset() # Discard recovery sample; new interval starts at its ending boundary.
$reading = Read-Counters 150 61 71
$accumulator.Add($reading.Sample)
Assert-Average $accumulator 0

# Model the baseline cleared by an API failure: mark the existing interval missing,
# establish a fresh monitor baseline, then explicitly reset at that boundary.
$accumulator.MarkMissing()
$monitor = [EcoWattSense.Client.Monitoring.CpuMonitor]::new()
$reading = Read-Counters 200 300 400
Assert-Check ($reading.Status.ToString() -eq 'BaselineEstablished') 'Recovery needs a fresh baseline.'
Assert-Check ($null -eq $accumulator.GetAveragePercentage()) 'Fresh baseline silently healed the interval.'
$accumulator.Reset()
$reading = Read-Counters 200 350 450
$accumulator.Add($reading.Sample)
Assert-Average $accumulator 100
Assert-Check (@([EcoWattSense.Client.Monitoring.CpuSample].GetProperties() | Where-Object CanWrite).Count -eq 0) 'Sample must be immutable.'
Write-Output "Passed $script:checks CPU interval checks. No native API calls were made."
