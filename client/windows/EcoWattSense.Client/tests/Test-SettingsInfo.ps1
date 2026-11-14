# Run with PowerShell 7 (.NET 10): pwsh -File ./tests/Test-SettingsInfo.ps1
# Compiles the real StatusReader, SettingsInfo and AutostartTask (no WPF) and checks the Settings page values from status.json:
# all fields, missing fields (an older service), wrong types, out of range numbers, the texts, and the autostart argument lists.
$ErrorActionPreference = 'Stop'
$root = Join-Path $PSScriptRoot '../../EcoWattSense.Client.Gui'
Add-Type -Path @((Join-Path $root 'Viewer/StatusReader.cs'), (Join-Path $root 'Settings/SettingsInfo.cs'), (Join-Path $root 'Settings/AutostartTask.cs'))
$reader = [EcoWattSense.Client.Gui.Settings.SettingsReader]
$text = [EcoWattSense.Client.Gui.Settings.SettingsText]
$checks = 0
# PowerShell turns a null string argument into an empty string, so a real null is passed explicitly.
$nullString = [System.Management.Automation.Language.NullString]::Value
function Check($ok, $what) { if (!$ok) { throw "Failed: $what" }; $script:checks++ }

# All fields.
$full = '{ "schema": 1, "service_version": "0.1.0", "level": "L2", "server_url": "https://example.org:8443", "server_key_set": true, "server_enabled": true, "device_type": "laptop", "interval_seconds": 300, "idle_threshold_seconds": 120, "unused_threshold_seconds": 900 }'
$s = $reader::Parse($full)
Check ($s.ServiceVersion -eq '0.1.0' -and $s.ServerUrl -eq 'https://example.org:8443' -and $s.KeySet -eq $true -and $s.ServerEnabled -eq $true) 'full file: server fields'
Check ($s.DeviceType -eq 'laptop' -and $s.IntervalSeconds -eq 300 -and $s.IdleSeconds -eq 120 -and $s.UnusedSeconds -eq 900) 'full file: computer fields'
Check ($text::Address($s.ServerUrl) -eq 'https://example.org:8443') 'address text'
Check ($text::Key($s.KeySet) -eq 'Server key is set') 'key text when set'
Check ($text::Sending($s.ServerEnabled) -eq 'On') 'sending on'
Check ($text::Device($s.DeviceType) -eq 'Laptop') 'device text'
Check ($text::Minutes($s.IntervalSeconds) -eq '5 min' -and $text::Minutes($s.IdleSeconds) -eq '2 min' -and $text::Minutes($s.UnusedSeconds) -eq '15 min') 'minutes'
Check ($text::Minutes(150) -eq '2.5 min') 'half minutes'
Check ($text::Version($s.ServiceVersion) -eq '0.1.0') 'version text'

# Not set, key not set, sending off, desktop.
$s = $reader::Parse('{ "server_url": "", "server_key_set": false, "server_enabled": false, "device_type": "desktop" }')
Check ($text::Address($s.ServerUrl) -eq 'Not set') 'empty URL is Not set'
Check ($text::Key($s.KeySet) -eq 'Server key is not set') 'key text when not set'
Check ($text::Sending($s.ServerEnabled) -eq 'Off') 'sending off'
Check ($text::Device($s.DeviceType) -eq 'Desktop') 'desktop text'

# An older service: none of the fields. Every value is unknown.
$old = $reader::Parse('{ "schema": 1, "service_version": "0.0.9", "level": "L1" }')
Check ($null -eq $old.ServerUrl -and $null -eq $old.KeySet -and $null -eq $old.ServerEnabled -and $null -eq $old.DeviceType -and $null -eq $old.IntervalSeconds) 'older service: fields are null'
Check ($text::Address($nullString) -eq 'unknown' -and $text::Key($old.KeySet) -eq 'unknown' -and $text::Sending($old.ServerEnabled) -eq 'unknown') 'older service: server texts'
Check ($text::Device($nullString) -eq 'unknown' -and $text::Minutes($old.IdleSeconds) -eq 'unknown') 'older service: computer texts'
Check ($old.ServiceVersion -eq '0.0.9') 'older service: version still read'
Check ($text::Version($nullString) -eq 'unknown' -and $text::Version('') -eq 'unknown') 'version unknown'

# Wrong types and out of range values are unknown, never a crash and never a guess.
$bad = $reader::Parse('{ "service_version": 3, "server_url": 5, "server_key_set": "yes", "server_enabled": 1, "device_type": "tablet", "interval_seconds": "300", "idle_threshold_seconds": 1.5, "unused_threshold_seconds": -4 }')
Check ($null -eq $bad.ServiceVersion -and $null -eq $bad.ServerUrl -and $null -eq $bad.KeySet -and $null -eq $bad.ServerEnabled) 'wrong types: server fields null'
Check ($null -eq $bad.DeviceType -and $null -eq $bad.IntervalSeconds -and $null -eq $bad.IdleSeconds -and $null -eq $bad.UnusedSeconds) 'wrong types: computer fields null'
$bad = $reader::Parse('{ "interval_seconds": 0, "idle_threshold_seconds": 99999999999, "unused_threshold_seconds": 604801, "server_url": "' + ('x' * 3000) + '", "service_version": "' + ('v' * 40) + '" }')
Check ($null -eq $bad.IntervalSeconds -and $null -eq $bad.IdleSeconds -and $null -eq $bad.UnusedSeconds) 'out of range seconds are null'
Check ($null -eq $bad.ServerUrl -and $null -eq $bad.ServiceVersion) 'too long texts are null'
Check ($null -eq $reader::Parse('{ "server_url": null, "server_key_set": null }').ServerUrl) 'JSON null is unknown'
Check ($reader::Parse('{ "interval_seconds": 604800 }').IntervalSeconds -eq 604800) 'upper limit accepted'

# Broken files: unknown everywhere.
foreach ($broken in @('{ not json', '[1,2]', '', '"text"', '5')) {
    $b = $reader::Parse($broken)
    Check ($null -eq $b.ServerUrl -and $null -eq $b.KeySet -and $null -eq $b.ServiceVersion) "broken content: $broken"
}

# Reading the file: missing file, oversize file, a good file.
$directory = Join-Path ([IO.Path]::GetTempPath()) ('EcoWattSense-SettingsTest-' + [guid]::NewGuid().ToString('N'))
try {
    $null = New-Item -ItemType Directory -Path $directory
    $path = Join-Path $directory 'status.json'
    Check ($null -eq $reader::Read($path).DeviceType) 'missing file'
    Set-Content -LiteralPath $path -Value ('{"device_type":"laptop","pad":"' + ('x' * 70000) + '"}') -NoNewline
    Check ($null -eq $reader::Read($path).DeviceType) 'oversize file is refused'
    Set-Content -LiteralPath $path -Value $full -NoNewline
    Check ($reader::Read($path).IntervalSeconds -eq 300) 'good file read from disk'
}
finally {
    $resolved = [IO.Path]::GetFullPath($directory)
    if (-not [IO.Path]::GetFileName($resolved).StartsWith('EcoWattSense-SettingsTest-')) { throw 'Unsafe cleanup path' }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}

# The key is never shown, whatever the file holds: a key field in the file does not reach any text.
$leak = $reader::Parse('{ "server_key": "SECRET-123", "server_key_set": true }')
Check ($text::Key($leak.KeySet) -eq 'Server key is set' -and $text::Key($leak.KeySet) -notmatch 'SECRET') 'the key never appears'

# Autostart argument lists: one item per argument, so a path with spaces needs no quotes.
$task = [EcoWattSense.Client.Gui.Settings.AutostartTask]
$enable = $task::EnableArguments('C:\Program Files\EcoWattSense\Install-Autostart.ps1', 'laptop', 'C:\Program Files\EcoWattSense\EcoWattSense.Client.Gui.exe')
Check (($enable -join '|') -eq '-NoProfile|-ExecutionPolicy|Bypass|-File|C:\Program Files\EcoWattSense\Install-Autostart.ps1|-DeviceType|laptop|-ExePath|C:\Program Files\EcoWattSense\EcoWattSense.Client.Gui.exe') 'enable arguments'
$disable = $task::DisableArguments('C:\Program Files\EcoWattSense\Uninstall-Autostart.ps1')
Check (($disable -join '|') -eq '-NoProfile|-ExecutionPolicy|Bypass|-File|C:\Program Files\EcoWattSense\Uninstall-Autostart.ps1') 'disable arguments'
Check ($task::TaskName -eq 'EcoWattSense Client') 'task name is the one of Install-Autostart.ps1'
$installScript = Get-Content -Raw (Join-Path $PSScriptRoot '../../scripts/Install-Autostart.ps1')
Check ($installScript -match "\`$TaskName = '$([regex]::Escape($task::TaskName))'") 'task name matches the default in Install-Autostart.ps1'
Write-Output "Passed $checks settings info checks."
