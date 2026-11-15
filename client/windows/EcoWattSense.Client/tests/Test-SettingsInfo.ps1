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

# The computer name line: the first line of device-name.txt, "Not set" when there is no usable name.
Check ($reader::ParseComputerName("Lab 1 PC 03`r`nignored") -eq 'Lab 1 PC 03') 'computer name: the first line, cleaned'
Check ($null -eq $reader::ParseComputerName($nullString) -and $null -eq $reader::ParseComputerName('  ') -and $null -eq $reader::ParseComputerName('')) 'computer name: no text is null'
Check ($null -eq $reader::ParseComputerName(('x' * 61)) -and $reader::ParseComputerName(('x' * 60)) -eq ('x' * 60)) 'computer name: 60 characters are the limit'
Check ($null -eq $reader::ParseComputerName("a`u{1}b")) 'computer name: control characters are refused'
Check ($text::ComputerName('Lab 1') -eq 'Lab 1' -and $text::ComputerName($nullString) -eq 'Not set' -and $text::ComputerName('') -eq 'Not set') 'computer name text'
$nameDirectory = Join-Path ([IO.Path]::GetTempPath()) ('EcoWattSense-SettingsTest-' + [guid]::NewGuid().ToString('N'))
try {
    $null = New-Item -ItemType Directory -Path $nameDirectory
    $namePath = Join-Path $nameDirectory 'device-name.txt'
    Check ($null -eq $reader::ReadComputerName($namePath)) 'computer name: missing file is null'
    [IO.File]::WriteAllText($namePath, "Lab 1 PC 03`n")
    Check ($reader::ReadComputerName($namePath) -eq 'Lab 1 PC 03') 'computer name: read from disk'
}
finally {
    $resolvedName = [IO.Path]::GetFullPath($nameDirectory)
    if (-not [IO.Path]::GetFileName($resolvedName).StartsWith('EcoWattSense-SettingsTest-')) { throw 'Unsafe cleanup path' }
    if (Test-Path -LiteralPath $resolvedName) { Remove-Item -LiteralPath $resolvedName -Recurse -Force }
}

# Autostart argument lists: one item per argument, so a path with spaces needs no quotes.
$task = [EcoWattSense.Client.Gui.Settings.AutostartTask]
$enable = $task::EnableArguments('C:\Program Files\EcoWattSense\Install-Autostart.ps1', 'laptop', 'C:\Program Files\EcoWattSense\EcoWattSense.Client.Gui.exe')
Check (($enable -join '|') -eq '-NoProfile|-ExecutionPolicy|Bypass|-File|C:\Program Files\EcoWattSense\Install-Autostart.ps1|-DeviceType|laptop|-ExePath|C:\Program Files\EcoWattSense\EcoWattSense.Client.Gui.exe') 'enable arguments'
$disable = $task::DisableArguments('C:\Program Files\EcoWattSense\Uninstall-Autostart.ps1')
Check (($disable -join '|') -eq '-NoProfile|-ExecutionPolicy|Bypass|-File|C:\Program Files\EcoWattSense\Uninstall-Autostart.ps1') 'disable arguments'
Check ($task::TaskName -eq 'EcoWattSense Client') 'task name is the one of Install-Autostart.ps1'
$installScript = Get-Content -Raw (Join-Path $PSScriptRoot '../../scripts/Install-Autostart.ps1')
Check ($installScript -match "\`$TaskName = '$([regex]::Escape($task::TaskName))'") 'task name matches the default in Install-Autostart.ps1'
# Who the autostart task runs for, read from the task XML (schtasks /query /xml). Task Scheduler writes a namespace.
$kind = [EcoWattSense.Client.Gui.Settings.AutostartKind]
$ns = 'xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task"'
$groupXml = "<Task $ns><Principals><Principal id=`"Author`"><GroupId>S-1-5-32-545</GroupId><RunLevel>LeastPrivilege</RunLevel></Principal></Principals></Task>"
$userXml = "<Task $ns><Principals><Principal id=`"Author`"><UserId>S-1-5-21-1-2-3-1001</UserId><LogonType>InteractiveToken</LogonType></Principal></Principals></Task>"
Check ($task::Parse($groupXml) -eq $kind::AllUsers) 'task XML with a group principal is AllUsers'
Check ($task::Parse($userXml) -eq $kind::User) 'task XML with a user principal is User'
Check ($task::Parse($nullString) -eq $kind::None -and $task::Parse('  ') -eq $kind::None) 'no task XML is None'
Check ($task::Parse('this is not xml') -eq $kind::Unknown) 'broken XML is Unknown'
Check ($task::Parse("<Task $ns/>") -eq $kind::User) 'task XML with no Principal is User'
# The "where do the settings come from" line (UTC zone, so the clock times are the same on every computer).
$utc = [TimeZoneInfo]::Utc
$lines = { param($json) $text::SettingsSourceLines($reader::Parse($json), $utc) }
$localLine = 'Local settings (not read from the server yet)'
$serverJson = '{ "settings_source": "server", "settings_version": 7, "last_settings_utc": "2026-11-15T21:26:42Z", "last_ping_utc": "2026-11-15T21:30:00Z", "last_ping_result": "ok" }'
Check ((& $lines $serverJson) -eq 'Settings from the server, version 7, checked 21:26') 'source line: server with all fields'
$failedJson = $serverJson.Replace('"last_ping_result": "ok"', '"last_ping_result": "timeout"')
Check ((& $lines $failedJson) -eq "Settings from the server, version 7, checked 21:26`n" + 'Server not reached at the last check (21:30).') 'source line: ping not ok adds a second line'
$noPingTime = '{ "settings_source": "server", "settings_version": 2, "last_settings_utc": "2026-11-15T08:05:00Z", "last_ping_result": "dns" }'
Check ((& $lines $noPingTime) -eq "Settings from the server, version 2, checked 08:05`n" + 'Server not reached at the last check.') 'source line: ping not ok without a time'
$noPingResult = '{ "settings_source": "server", "settings_version": 7, "last_settings_utc": "2026-11-15T21:26:42Z" }'
Check ((& $lines $noPingResult) -eq 'Settings from the server, version 7, checked 21:26') 'source line: no ping result adds nothing'
Check ((& $lines '{ "settings_source": "local", "settings_version": 7, "last_settings_utc": "2026-11-15T21:26:42Z" }') -eq $localLine) 'source line: local'
Check ((& $lines '{ "service_version": "0.1.0" }') -eq $localLine) 'source line: fields missing (older service)'
Check ((& $lines '{ "settings_source": "server" }') -eq $localLine) 'source line: server without version and time is local'
Check ((& $lines '{ "settings_source": 5, "settings_version": "7", "last_settings_utc": 12 }') -eq $localLine) 'source line: wrong types'
Check ((& $lines '{ "settings_source": "server", "settings_version": 7.5, "last_settings_utc": "2026-11-15T21:26:42Z" }') -eq $localLine) 'source line: fractional version is local'
Check ((& $lines '{ "settings_source": "server", "settings_version": 7, "last_settings_utc": "yesterday" }') -eq $localLine) 'source line: bad time text is local'
Check ((& $lines '{ not json') -eq $localLine) 'source line: broken file'
$zone = [TimeZoneInfo]::CreateCustomTimeZone('Test+2', [TimeSpan]::FromHours(2), 'Test+2', 'Test+2')
Check ($text::SettingsSourceLines($reader::Parse($serverJson), $zone) -eq 'Settings from the server, version 7, checked 23:26') 'source line: shown in the given time zone'
Write-Output "Passed $checks settings info checks."
