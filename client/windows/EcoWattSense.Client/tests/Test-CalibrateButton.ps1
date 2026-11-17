# Run with PowerShell 7 (.NET 10): pwsh -File ./tests/Test-CalibrateButton.ps1
# Compiles the real CalibrateButtonState (no WPF) and checks when the Calibrate button is enabled and each reason line.
$ErrorActionPreference = 'Stop'
$viewer = Join-Path $PSScriptRoot '../../EcoWattSense.Client.Gui/Viewer'
Add-Type -Path (Join-Path $viewer 'CalibrateButtonState.cs')
$checks = 0
function Check($ok, $what) { if (!$ok) { throw "Failed: $what" }; $script:checks++ }
$nullBool = [Nullable[bool]]$null
$nullInt = [Nullable[int]]$null
function Decide($battery, $charge, $found, $running) { [EcoWattSense.Client.Gui.Viewer.CalibrateButtonState]::Decide($battery, $charge, $found, $running) }
function Expect($state, $enabled, $reason, $what) { Check (($state.Enabled -eq $enabled) -and ($state.Reason -eq $reason)) "$what (got $($state.Enabled), '$($state.Reason)')" }

Expect (Decide $true 80 $true $false) $true '' 'battery, 80 %, found, idle: enabled'
Expect (Decide $true 30 $true $false) $true '' 'exactly 30 %: enabled'
Expect (Decide $true 29 $true $false) $false 'Battery below 30 %.' '29 %'
Expect (Decide $true 0 $true $false) $false 'Battery below 30 %.' '0 %'
Expect (Decide $true $nullInt $true $false) $false 'Battery below 30 %.' 'unknown charge counts as too low'
Expect (Decide $false 80 $true $false) $false 'Unplug the charger to calibrate.' 'on mains'
Expect (Decide $nullBool 80 $true $false) $false 'Unplug the charger to calibrate.' 'unknown power source'
Expect (Decide $true 80 $false $false) $false 'Calibrator not found.' 'calibrator missing'
Expect (Decide $true 80 $true $true) $false 'Calibration is running.' 'calibrator running'
# The order of the reasons: running, then missing, then power source, then charge.
Expect (Decide $false 10 $false $true) $false 'Calibration is running.' 'running wins over everything'
Expect (Decide $false 10 $false $false) $false 'Calibrator not found.' 'missing wins over mains and charge'
Expect (Decide $false 10 $true $false) $false 'Unplug the charger to calibrate.' 'mains wins over charge'
Check ([EcoWattSense.Client.Gui.Viewer.CalibrateButtonState]::MinimumChargePercent -eq 30) 'minimum charge is 30'
Write-Output "Passed $checks calibrate button checks."
