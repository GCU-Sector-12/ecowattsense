# Run with PowerShell 7 (.NET 10): pwsh -File ./tests/Test-Rename.ps1
# Compiles the real StatusReader, SettingsInfo and DeviceNameRename (no WPF) and checks the rename feature of the Settings page:
# name validation, the exact pipe text and its 256 byte limit, the status lines, and missing or wrong-typed status.json fields.
$ErrorActionPreference = 'Stop'
$root = Join-Path $PSScriptRoot '../../EcoWattSense.Client.Gui'
Add-Type -Path @((Join-Path $root 'Viewer/StatusReader.cs'), (Join-Path $root 'Settings/SettingsInfo.cs'), (Join-Path $root 'Settings/DeviceNameRename.cs'))
$rules = [EcoWattSense.Client.Gui.Settings.DeviceNameRules]
$lines = [EcoWattSense.Client.Gui.Settings.DeviceNameLines]
$reader = [EcoWattSense.Client.Gui.Settings.SettingsReader]
$nullString = [System.Management.Automation.Language.NullString]::Value
$checks = 0
function Check($ok, $what) { if (!$ok) { throw "Failed: $what" }; $script:checks++ }
$guillemets = [char]0x00AB, [char]0x00BB
function Kept($name) { "The server keeps the name $($guillemets[0])$name$($guillemets[1]). An admin can change it in the dashboard." }

# Name validation.
$r = $rules::Check($nullString)
Check ($r.Valid -and $r.Name -eq '' -and $r.Reason -eq '') 'null is an empty valid name'
$r = $rules::Check('   ')
Check ($r.Valid -and $r.Name -eq '') 'spaces only trim to empty'
$r = $rules::Check('  Office PC  ')
Check ($r.Valid -and $r.Name -eq 'Office PC') 'name is trimmed'
Check ($rules::Check('a' * 60).Valid) '60 characters are valid'
$r = $rules::Check('a' * 61)
Check (!$r.Valid -and $r.Reason -eq 'Use at most 60 characters.') '61 characters are not valid'
Check ($rules::Check(('a' * 60) + '   ').Valid) 'trailing spaces do not count'
$polish = 'Zażółć gęślą jaźń'
Check ($rules::Check($polish).Valid -and $rules::Check($polish).Name -eq $polish) 'Polish letters are valid'
Check ($rules::Check('ą' * 60).Valid -and !$rules::Check('ą' * 61).Valid) 'Polish letters count as one character each'
$emoji = [char]::ConvertFromUtf32(0x1F600)
Check ($rules::Check($emoji * 45).Valid) '45 code points, one UTF-16 pair each, are valid and fit the message'
Check (!$rules::Check($emoji * 61).Valid) '61 code points are not valid'
$r = $rules::Check($emoji * 60)
Check (!$r.Valid -and $r.Reason -eq 'This name is too long to send.') '60 four-byte code points are too long for 256 bytes'
$r = $rules::Check("a$([char]1)b")
Check (!$r.Valid -and $r.Reason -eq 'Remove the control characters.') 'control character is not valid'
Check (!$rules::Check("a`nb").Valid) 'line feed inside is not valid'

# The exact pipe text and the byte limit.
Check ($rules::Message('Office PC') -eq "EWS1 name=T2ZmaWNlIFBD`n") 'exact message for a name'
Check ($rules::Message('') -eq "EWS1 name=`n") 'empty name: nothing after name='
$utf8 = [System.Text.Encoding]::UTF8
$polishMessage = $rules::Message($polish)
Check ($polishMessage -eq ('EWS1 name=' + [Convert]::ToBase64String($utf8.GetBytes($polish)) + "`n")) 'Polish name is base64 of UTF-8'
Check ($utf8.GetByteCount($rules::Message('a' * 60)) -le 256) '60 ASCII characters fit'
Check ($null -ne $rules::Message('a' * 183)) '183 bytes give 255 message bytes: fits'
Check ($null -eq $rules::Message('a' * 184)) '184 bytes give 259 message bytes: too long'
Check ($utf8.GetByteCount($rules::Message('a' * 183)) -le 256) 'message of 183 bytes is at most 256 bytes'

# Status lines.
Check ($lines::Line('Office', $nullString, 'pending') -eq 'Waiting to send the name to the server.') 'pending line'
Check ($lines::Line('Office', 'Office', 'failed') -eq 'The server did not accept the name.') 'failed line'
Check ($lines::Line('Office', 'Office', 'ok') -eq '') 'ok and same: no line'
Check ($lines::Line('Office', 'Admin name', 'ok') -eq (Kept 'Admin name')) 'ok and different: server keeps line'
Check ($lines::Line('Office', $nullString, 'ok') -eq (Kept '')) 'ok, server has none, name set: line'
Check ($lines::Line('', $nullString, 'ok') -eq '') 'ok, both empty: no line'
Check ($lines::Line('', 'Admin name', 'ok') -eq (Kept 'Admin name')) 'ok, local empty, server has a name: line'
Check ($lines::Line($nullString, 'Admin name', 'ok') -eq '') 'device name unknown: no line'
Check ($lines::Line('Office', 'Admin name', $nullString) -eq '') 'sync unknown: no line'
Check ($lines::Line('Office', 'Admin name', 'weird') -eq '') 'sync unknown word: no line'

# status.json fields.
$s = $reader::Parse('{ "device_name": "Office", "server_device_name": "Admin name", "name_sync": "ok" }')
Check ($s.DeviceName -eq 'Office' -and $s.ServerDeviceName -eq 'Admin name' -and $s.NameSync -eq 'ok') 'all three fields read'
$s = $reader::Parse('{ "device_name": "", "server_device_name": null, "name_sync": "pending" }')
Check ($s.DeviceName -eq '' -and $null -eq $s.ServerDeviceName -and $s.NameSync -eq 'pending') 'empty name, JSON null server name'
$s = $reader::Parse('{ "service_version": "0.0.9" }')
Check ($null -eq $s.DeviceName -and $null -eq $s.ServerDeviceName -and $null -eq $s.NameSync) 'older service: fields null'
$s = $reader::Parse('{ "device_name": 5, "server_device_name": true, "name_sync": 1 }')
Check ($null -eq $s.DeviceName -and $null -eq $s.ServerDeviceName -and $null -eq $s.NameSync) 'wrong types are null'
$s = $reader::Parse('{ "device_name": "' + ('a' * 61) + '", "server_device_name": "a\u0001b", "name_sync": "done" }')
Check ($null -eq $s.DeviceName -and $null -eq $s.ServerDeviceName -and $null -eq $s.NameSync) 'too long, control character and unknown sync word are null'
Check ($null -ne $reader::Parse('{ "device_name": "' + ('a' * 60) + '" }').DeviceName) '60 characters in status.json are accepted'
Write-Output "Passed $checks rename checks."
