# Run with PowerShell 7 (.NET 10): pwsh -File ./tests/Test-UninstallButton.ps1
# Compiles the real UninstallButton (no WPF) and checks the state of the Uninstall button (Setup found or not) and the Setup arguments.
$ErrorActionPreference = 'Stop'
$settings = Join-Path $PSScriptRoot '../../EcoWattSense.Client.Gui/Settings'
Add-Type -Path (Join-Path $settings 'UninstallButton.cs')
$checks = 0
function Check($ok, $what) { if (!$ok) { throw "Failed: $what" }; $script:checks++ }
$button = [EcoWattSense.Client.Gui.Settings.UninstallButton]
$found = $button::Decide($true)
$missing = $button::Decide($false)
Check ($found.Enabled -and $found.Note -eq '') 'Setup found: the button is enabled and has no note'
Check ((-not $missing.Enabled) -and $missing.Note -eq 'Setup.exe not found.') 'Setup missing: the button is disabled with the note'
Check ($button::SetupArguments -eq '/uninstall') 'Setup is started with /uninstall'
Write-Output "Passed $checks uninstall button checks."
