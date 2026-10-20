# Run with PowerShell 7 (.NET 10): pwsh -File ./tests/Test-StateClassifier.ps1
# Compiles the actual pure classifier without starting the Windows monitors or adding packages.
$ErrorActionPreference = 'Stop'
Add-Type -Path (Join-Path $PSScriptRoot '../Monitoring/StateClassifier.cs')
$classifier = [EcoWattSense.Client.Monitoring.StateClassifier]::new()
$checks = 0

foreach ($case in @(
    @{ Seconds = 0; Expected = 'Active' },
    @{ Seconds = 599.999; Expected = 'Active' },
    @{ Seconds = 600; Expected = 'Idle' },
    @{ Seconds = 600.001; Expected = 'Idle' },
    @{ Seconds = 3599.999; Expected = 'Idle' },
    @{ Seconds = 3600; Expected = 'Unused' },
    @{ Seconds = 3600.001; Expected = 'Unused' },
    @{ Seconds = [double]::MaxValue; Expected = 'Unused' },
    @{ Seconds = $null; Expected = $null },
    @{ Seconds = -1; Expected = $null },
    @{ Seconds = [double]::NaN; Expected = $null },
    @{ Seconds = [double]::PositiveInfinity; Expected = $null },
    @{ Seconds = [double]::NegativeInfinity; Expected = $null }
)) {
    $actual = $classifier.Classify($case.Seconds)
    if ($null -eq $case.Expected) {
        if ($null -ne $actual) { throw "Expected unknown for $($case.Seconds), got $actual" }
    } elseif ($actual.ToString() -ne $case.Expected) {
        throw "Expected $($case.Expected) for $($case.Seconds), got $actual"
    }
    $checks++
}

foreach ($pair in @(
    @(0, 3600), @(-1, 3600), @(600, 600), @(600, 599), @(600, 0),
    @([double]::NaN, 3600), @([double]::PositiveInfinity, 3600),
    @([double]::NegativeInfinity, 3600), @(600, [double]::NaN),
    @(600, [double]::PositiveInfinity), @(600, [double]::NegativeInfinity)
)) {
    $rejected = $false
    try { $null = [EcoWattSense.Client.Monitoring.StateClassifier]::new($pair[0], $pair[1]) }
    catch {
        if ($_.Exception.GetBaseException() -isnot [ArgumentOutOfRangeException]) { throw }
        $rejected = $true
    }
    if (!$rejected) { throw "Accepted invalid thresholds: $pair" }
    $checks++
}

$custom = [EcoWattSense.Client.Monitoring.StateClassifier]::new(10, 20)
if ($custom.Classify(9.999).ToString() -ne 'Active' -or
    $custom.Classify(10).ToString() -ne 'Idle' -or
    $custom.Classify(20).ToString() -ne 'Unused' -or
    $custom.Classify(0).ToString() -ne 'Active') {
    throw 'Custom thresholds or return to Active failed.'
}
$checks++
if ([int][EcoWattSense.Client.Monitoring.ActivityState]::Active -ne 2 -or
    [int][EcoWattSense.Client.Monitoring.ActivityState]::Idle -ne 1 -or
    [int][EcoWattSense.Client.Monitoring.ActivityState]::Unused -ne 0) {
    throw 'State numeric values are incorrect.'
}
$checks++
Write-Output "Passed $checks classifier checks."
