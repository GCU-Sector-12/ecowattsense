# Run with PowerShell 7 (.NET 10): pwsh -File ./tests/Test-Update.ps1
# Compiles the real UpdateRules (no WPF, no network) and checks the manifest rules, the version compare, the redirect host
# allowlist, the SHA256 compare, the "seen version" rule and the texts of the Updates section.
$ErrorActionPreference = 'Stop'
$update = Join-Path $PSScriptRoot '../../EcoWattSense.Client.Gui/Update'
Add-Type -Path (Join-Path $update 'UpdateRules.cs')
$checks = 0
function Check($ok, $what) { if (!$ok) { throw "Failed: $what" }; $script:checks++ }
$rules = [EcoWattSense.Client.Gui.Update.UpdateRules]
$state = [EcoWattSense.Client.Gui.Update.UpdateState]
$hash = 'a' * 64
$zip = 'https://github.com/GCU-Sector-12/ecowattsense/releases/download/v0.1.1/EcoWattSense.zip'

function Manifest($version, $zipUrl, $sha, $size) {
    $object = [ordered]@{ version = $version; zip_url = $zipUrl; sha256 = $sha; size = $size }
    return $rules::ParseManifest(($object | ConvertTo-Json -Compress))
}

# Manifest
$good = Manifest '0.1.1' $zip $hash 1000
Check ($null -ne $good -and $good.Version -eq '0.1.1' -and $good.Size -eq 1000) 'a good manifest is accepted'
Check ($good.Sha256 -eq $hash) 'the hash is kept in lower case'
Check ($null -eq (Manifest 'v0.1.1' $zip $hash 1000)) 'a version with a letter is refused'
Check ($null -eq (Manifest '' $zip $hash 1000)) 'an empty version is refused'
Check ($null -eq (Manifest '0.1.1' 'http://github.com/GCU-Sector-12/ecowattsense/releases/download/a.zip' $hash 1000)) 'http is refused'
Check ($null -eq (Manifest '0.1.1' 'https://evil.example/GCU-Sector-12/ecowattsense/releases/download/a.zip' $hash 1000)) 'a foreign host is refused'
Check ($null -eq (Manifest '0.1.1' 'https://github.com/other/repo/releases/download/a.zip' $hash 1000)) 'a foreign repository is refused'
Check ($null -eq (Manifest '0.1.1' $zip ('a' * 63) 1000)) 'a short hash is refused'
Check ($null -eq (Manifest '0.1.1' $zip ('g' * 64) 1000)) 'a hash that is not hex is refused'
Check ($null -eq (Manifest '0.1.1' $zip $hash 0)) 'size 0 is refused'
Check ($null -eq (Manifest '0.1.1' $zip $hash 314572801)) 'size above 300 MB is refused'
Check ($null -ne (Manifest '0.1.1' $zip $hash 314572800)) 'size of exactly 300 MB is accepted'
Check ($null -eq (Manifest '0.1.1' $zip $hash '1000')) 'a size that is text is refused'
Check ($null -eq (Manifest '0.1.1' $zip $hash 1.5)) 'a size that is not whole is refused'
Check ($null -eq $rules::ParseManifest('not json')) 'text that is not JSON is refused'
Check ($null -eq $rules::ParseManifest('[1,2]')) 'JSON that is not an object is refused'
Check ($null -eq $rules::ParseManifest('{"version":"0.1.1"}')) 'missing fields are refused'

# Version compare
Check ($rules::CompareVersions('0.1.0', '0.1.1') -lt 0) '0.1.0 < 0.1.1'
Check ($rules::CompareVersions('0.1.1', '0.2.0') -lt 0) '0.1.1 < 0.2.0'
Check ($rules::CompareVersions('0.2.0', '1.0.0') -lt 0) '0.2.0 < 1.0.0'
Check ($rules::CompareVersions('0.9.0', '0.10.0') -lt 0) '0.9.0 < 0.10.0 (numbers, not text)'
Check ($rules::CompareVersions('1.0.0', '0.2.0') -gt 0) '1.0.0 > 0.2.0'
Check ($rules::CompareVersions('0.1.0', '0.1.0') -eq 0) 'equal versions'
Check ($rules::CompareVersions('1.0', '1.0.0') -eq 0) 'a missing part counts as 0'
Check ($null -eq $rules::CompareVersions('0.1.0', 'abc')) 'bad text on the right gives null'
Check ($null -eq $rules::CompareVersions('1.x', '0.1.0')) 'bad text on the left gives null'
Check ($null -eq $rules::CompareVersions('', '0.1.0')) 'an empty text gives null'
Check ($rules::StripBuild('0.1.0+abc123') -eq '0.1.0') 'the +build suffix is removed'
Check ($rules::StripBuild('0.1.0') -eq '0.1.0') 'a version without a suffix is unchanged'

# Redirect hosts
function Allowed($address) { return $rules::IsAllowedRedirect([Uri]$address) }
Check (Allowed 'https://github.com/a/b') 'github.com is allowed'
Check (Allowed 'https://objects.githubusercontent.com/x') 'objects.githubusercontent.com is allowed'
Check (Allowed 'https://release-assets.githubusercontent.com/x') 'release-assets.githubusercontent.com is allowed'
Check (-not (Allowed 'http://github.com/a')) 'http is not allowed'
Check (-not (Allowed 'https://evil.example/a')) 'another host is not allowed'
Check (-not (Allowed 'https://github.com.evil.example/a')) 'a look-alike host is not allowed'
Check (-not (Allowed 'https://githubusercontent.com/a')) 'the bare githubusercontent.com is not allowed'
Check (-not (Allowed 'https://github.com:8443/a')) 'another port is not allowed'
Check (-not (Allowed 'https://user@github.com/a')) 'user info is not allowed'
Check (-not $rules::IsAllowedRedirect($null)) 'null is not allowed'

# SHA256
$bytes = [System.Text.Encoding]::UTF8.GetBytes('hello')
$actual = [System.Security.Cryptography.SHA256]::HashData($bytes)
$hex = [Convert]::ToHexString($actual)
Check ($rules::Sha256Matches($hex.ToLowerInvariant(), $actual)) 'the same hash in lower case matches'
Check ($rules::Sha256Matches($hex, $actual)) 'the same hash in upper case matches'
Check (-not $rules::Sha256Matches(('0' * 64), $actual)) 'another hash does not match'

# Seen version
Check ($rules::ShouldNotify('0.1.1', $null)) 'no file: the balloon shows'
Check ($rules::ShouldNotify('0.1.1', '')) 'an empty file: the balloon shows'
Check ($rules::ShouldNotify('0.1.1', '0.1.0')) 'an older seen version: the balloon shows'
Check (-not $rules::ShouldNotify('0.1.1', '0.1.1')) 'the same version: no balloon'
Check (-not $rules::ShouldNotify('0.1.1', "0.1.1`r`n")) 'the same version with a line end: no balloon'

# State and section text
$newer = Manifest '0.1.1' $zip $hash 1000
Check ($rules::StateFor('0.1.0', $newer) -eq $state::Available) 'a newer manifest gives Available'
Check ($rules::StateFor('0.1.1', $newer) -eq $state::UpToDate) 'the same version gives UpToDate'
Check ($rules::StateFor('0.2.0', $newer) -eq $state::UpToDate) 'an older manifest gives UpToDate'
Check ($rules::StateFor('0.1.0', $null) -eq $state::Failed) 'no manifest gives Failed'
Check ($rules::StateFor('', $newer) -eq $state::Failed) 'an unknown own version gives Failed'
$view = $rules::View('0.1.0', $state::Available, '0.1.1')
Check ($view.VersionLine -eq 'This version 0.1.0' -and $view.StatusLine -eq 'Update available: 0.1.1' -and $view.ShowButton) 'Available text and button'
$view = $rules::View('0.1.0', $state::UpToDate, $null)
Check ($view.StatusLine -eq 'Up to date.' -and -not $view.ShowButton) 'UpToDate text, no button'
$view = $rules::View('0.1.0', $state::Failed, $null)
Check ($view.StatusLine -eq 'Could not check for updates.' -and -not $view.ShowButton) 'Failed text, no button'
$view = $rules::View('0.1.0', $state::Checking, $null)
Check ($view.StatusLine -eq 'Checking for updates...' -and -not $view.ShowButton) 'Checking text, no button'
Check ($rules::ProgressLine(1572864, 42163200) -eq 'Downloading... 1.5 of 40.2 MB') 'the progress line'

Write-Output "Passed $checks update checks."
