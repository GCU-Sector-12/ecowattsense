# Run with PowerShell 7 (.NET 10): pwsh -File ./tests/Test-ProgramLocator.ps1
# Compiles the real ProgramLocator (no WPF) and checks the lookup order for the calibrator, Setup and the autostart scripts:
# the client's own folder first, then Program Files\EcoWattSense, else null. The disk is not touched: a fake "exists" is used.
$ErrorActionPreference = 'Stop'
$settings = Join-Path $PSScriptRoot '../../EcoWattSense.Client.Gui/Settings'
Add-Type -Path (Join-Path $settings 'ProgramLocator.cs')
$checks = 0
function Check($ok, $what) { if (!$ok) { throw "Failed: $what" }; $script:checks++ }
$locator = [EcoWattSense.Client.Gui.Settings.ProgramLocator]
$client = 'C:\Apps\Client'
$programFiles = 'C:\Program Files'
function Find($name, $present) {
    $set = @($present)
    $exists = [Func[string, bool]] { param($path) $set -contains $path }.GetNewClosure()
    $locator::Find($name, $client, $programFiles, $exists)
}

foreach ($name in @($locator::CalibratorName, $locator::SetupName, $locator::InstallAutostartName, $locator::UninstallAutostartName)) {
    $own = Join-Path $client $name
    $installed = Join-Path (Join-Path $programFiles 'EcoWattSense') $name
    Check ((Find $name @($own)) -eq $own) "$name next to the client"
    Check ((Find $name @($installed)) -eq $installed) "$name only in Program Files"
    Check ((Find $name @($own, $installed)) -eq $own) "$name in both: the client's folder wins"
    Check ($null -eq (Find $name @())) "$name nowhere"
    Check ($null -eq (Find $name @((Join-Path $client 'Other.exe')))) "${name}: another file does not count"
}
Check ($locator::CalibratorName -eq 'EnergyCalibrator.exe') 'calibrator file name'
Check ($locator::SetupName -eq 'Setup.exe') 'Setup file name'
Write-Output "Passed $checks program locator checks."
