# Build-Package.ps1: builds the release package of the energy suite.
# Who uses it: the person who makes a release. Nothing is signed and nothing is installed.
# How to run: pwsh -NoProfile -File Build-Package.ps1 [-Version 0.1.0] [-OutDir <folder>] [-ReleaseTag <tag>] [-SkipWebSetup] [-Sign [-PfxPath <file>]]
# Result in <OutDir>:
#   EcoWattSense\                          all five programs, one shared .NET runtime, and README.md next to Setup.exe;
#   EcoWattSense-<Version>-win-x64.zip     the folder above, with its .sha256;
#   EcoWattSense-Web-Setup.exe + .sha256   the small web installer, NEXT TO the ZIP and not inside it;
#   latest.json                            {"version","zip_url","sha256","size","server_url"} that clients read (folder release\ of the suite).
# -Version sets the version of every program (C# through -p:Version, native through Build-Native.ps1 -Version).
# -ReleaseTag is the GitHub release tag used in zip_url; the default is v<Version>-energy-suite.
# -ServerUrl: the server address written as "server_url" into latest.json (default https://dev.ecowattsense.co.uk).
# -SkipWebSetup: a missing EcoWattSense-Web-Setup.exe is fine; without it the script stops when the file is missing.
# -Sign: signs our own programs and the web installer with a code signing certificate BEFORE the ZIP and the sums are made.
#   The .pfx comes from -PfxPath or the environment variable ENERGY_SUITE_SIGNING_PFX_PATH; its password ONLY from the environment
#   variable ENERGY_SUITE_SIGNING_PASSWORD (never a parameter, so it is never on a command line). The certificate is imported into
#   Cert:\CurrentUser\My for the run and removed again, so signtool can use /sha1 and no password reaches its command line.
#   Without -Sign nothing is signed and nothing changes. See docs\SIGNING.md.
# Exit codes: 0 package made; 1 a step failed or the folder is incomplete;
#             3 the Setup project is missing (it is written separately; the script does not create it);
#             4 the web installer was not built and -SkipWebSetup was not given;
#             5 signing was asked for and failed (missing .pfx, password or signtool.exe, a signing error, or a wrong signer).
param(
    [string] $Version = '0.1.0',
    [string] $OutDir = '',
    [string] $ReleaseTag = '',
    [string] $ServerUrl = 'https://dev.ecowattsense.co.uk',
    [switch] $SkipWebSetup,
    [switch] $Sign,
    [string] $PfxPath = ''
)

$ErrorActionPreference = 'Stop'
$suite = Split-Path -Parent $PSScriptRoot
$repo = (Resolve-Path (Join-Path $suite '..\..\..')).Path
if ($OutDir -eq '') { $OutDir = Join-Path $repo 'out\package' }
if ($ReleaseTag -eq '') { $ReleaseTag = "v$Version-energy-suite" }

$clientProject = Join-Path $repo 'client\windows\EcoWattSense.Client.Gui\EcoWattSense.Client.Gui.csproj'
$calibratorProject = Join-Path $repo 'client\spike\energy-calibrator\EnergyCalibrator.csproj'
$setupProject = Join-Path $suite 'setup\EcoWattSense.Setup\EcoWattSense.Setup.csproj'
$nativeOut = Join-Path $suite 'native\out\Release'
$webSetupSource = Join-Path $nativeOut 'EcoWattSense-Web-Setup.exe'
$stage = Join-Path $OutDir 'EcoWattSense'
$zipPath = Join-Path $OutDir "EcoWattSense-$Version-win-x64.zip"
$requiredFiles = 'ews-service.exe', 'ews-scan.exe', 'EcoWattSense.Client.Gui.exe', 'EnergyCalibrator.exe', 'Setup.exe'
# Our own files only. The .NET runtime files in the same folder are Microsoft's and stay as they are.
$ownFiles = 'ews-service.exe', 'ews-scan.exe', 'Setup.exe', 'EcoWattSense.Client.Gui.exe', 'EcoWattSense.Client.exe', 'EnergyCalibrator.exe',
    'EcoWattSense.Client.Gui.dll', 'EcoWattSense.Client.dll', 'EnergyCalibrator.dll', 'EcoWattSense.Setup.dll'
$timestampUrl = 'http://timestamp.digicert.com'

<#
.SYNOPSIS Stops the script with a message and an exit code.
.PARAMETER Message What went wrong, in plain words.
.PARAMETER Code The exit code.
#>
function Stop-Package {
    param([string] $Message, [int] $Code = 1)
    [Console]::Error.WriteLine("Build-Package: $Message")
    exit $Code
}

<#
.SYNOPSIS Publishes one C# project self-contained for win-x64 into the shared folder.
.PARAMETER Project Full path of the .csproj.
.DESCRIPTION Runtime files that several programs share are written once: the same file is simply overwritten.
#>
function Publish-Project {
    param([string] $Project)
    & dotnet publish $Project -c Release -r win-x64 --self-contained true -p:Version=$Version -p:IncludeSourceRevisionInInformationalVersion=false -p:DebugType=none -o $stage --nologo -v:minimal
    if ($LASTEXITCODE -ne 0) { Stop-Package "dotnet publish failed for $Project (exit $LASTEXITCODE)." }
}

<#
.SYNOPSIS Finds the newest x64 signtool.exe of the Windows 10/11 SDK.
.OUTPUTS The full path, or $null when no SDK is installed.
#>
function Find-SignTool {
    $root = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
    $found = Get-ChildItem -Path $root -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' }
    $newest = $found | Sort-Object { [version] $_.Name } | Select-Object -Last 1
    if (-not $newest) { return $null }
    $path = Join-Path $newest.FullName 'x64\signtool.exe'
    if (Test-Path -LiteralPath $path) { return $path }
    return $null
}

<#
.SYNOPSIS Imports the .pfx into Cert:\CurrentUser\My so that signtool can find it by thumbprint.
.PARAMETER Pfx Full path of the .pfx file.
.PARAMETER Password The .pfx password as a SecureString.
.OUTPUTS An object with Thumbprint and Added ($true when this call put the certificate in the store, so that it may be removed again).
.NOTES Raises an error when the file or the password is wrong.
#>
function Import-SigningPfx {
    param([string] $Pfx, [securestring] $Password)
    $flags = [System.Security.Cryptography.X509Certificates.X509KeyStorageFlags]::DefaultKeySet
    $probe = [System.Security.Cryptography.X509Certificates.X509Certificate2]::new($Pfx, $Password, $flags)
    $thumbprint = $probe.Thumbprint
    $probe.Dispose()
    $already = Test-Path -LiteralPath "Cert:\CurrentUser\My\$thumbprint"
    if (-not $already) { Import-PfxCertificate -FilePath $Pfx -CertStoreLocation 'Cert:\CurrentUser\My' -Password $Password | Out-Null }
    return [pscustomobject]@{ Thumbprint = $thumbprint; Added = (-not $already) }
}

<#
.SYNOPSIS Signs the given files with the certificate that has the given thumbprint in Cert:\CurrentUser\My.
.PARAMETER SignTool Full path of signtool.exe.
.PARAMETER Thumbprint Thumbprint of the signing certificate.
.PARAMETER Files Full paths of the files to sign.
.PARAMETER TimestampUrl RFC 3161 timestamp server, so that signatures stay valid after the certificate expires.
.NOTES Raises an error when signtool fails for any file.
#>
function Invoke-SignFiles {
    param([string] $SignTool, [string] $Thumbprint, [string[]] $Files, [string] $TimestampUrl)
    foreach ($file in $Files) {
        & $SignTool sign /fd SHA256 /tr $TimestampUrl /td SHA256 /s My /sha1 $Thumbprint /q $file
        if ($LASTEXITCODE -ne 0) { throw "signtool failed for $file (exit $LASTEXITCODE)." }
    }
}

<#
.SYNOPSIS Checks that every file has a signature made by the certificate with the given thumbprint.
.DESCRIPTION Uses Get-AuthenticodeSignature and compares the signer thumbprint only. The trust status is not checked, because the
  self-signed root may be untrusted on the build machine.
.PARAMETER Files Full paths of the files to check.
.PARAMETER Thumbprint Expected signer thumbprint.
.OUTPUTS The names of files with a missing or wrong signature (empty when all are fine).
#>
function Get-WronglySigned {
    param([string[]] $Files, [string] $Thumbprint)
    $bad = @()
    foreach ($file in $Files) {
        $signer = (Get-AuthenticodeSignature -LiteralPath $file).SignerCertificate
        $actual = if ($signer) { $signer.Thumbprint } else { '' }
        if ($actual -ne $Thumbprint) { $bad += (Split-Path -Leaf $file) }
    }
    return $bad
}

<#
.SYNOPSIS Imports the .pfx, signs the files, checks the signatures and removes the certificate from the store again.
.PARAMETER Files Full paths of the files to sign.
.PARAMETER Pfx Full path of the .pfx.
.PARAMETER PasswordText The .pfx password read from the environment.
.PARAMETER TimestampUrl RFC 3161 timestamp server.
.NOTES Raises an error on any failure; the certificate is removed even then.
#>
function Invoke-SigningRun {
    param([string[]] $Files, [string] $Pfx, [string] $PasswordText, [string] $TimestampUrl)
    $signTool = Find-SignTool
    if (-not $signTool) { throw 'signtool.exe not found. Install the Windows 10/11 SDK (Signing Tools for Desktop Apps).' }
    $secure = ConvertTo-SecureString -String $PasswordText -AsPlainText -Force
    $imported = Import-SigningPfx -Pfx $Pfx -Password $secure
    try {
        Invoke-SignFiles -SignTool $signTool -Thumbprint $imported.Thumbprint -Files $Files -TimestampUrl $TimestampUrl
        $bad = Get-WronglySigned -Files $Files -Thumbprint $imported.Thumbprint
        if ($bad) { throw "wrong or missing signature on: $($bad -join ', ')." }
    }
    finally {
        if ($imported.Added) { Remove-Item -LiteralPath "Cert:\CurrentUser\My\$($imported.Thumbprint)" -Force -ErrorAction SilentlyContinue }
    }
}

# Signing settings are checked first, so that a missing .pfx does not waste a whole build.
if ($Sign -and $PfxPath -eq '') { $PfxPath = $env:ENERGY_SUITE_SIGNING_PFX_PATH }
if ($Sign -and ([string]::IsNullOrEmpty($PfxPath) -or -not (Test-Path -LiteralPath $PfxPath))) { Stop-Package '-Sign needs a .pfx: give -PfxPath or set ENERGY_SUITE_SIGNING_PFX_PATH to an existing file.' 5 }
if ($Sign -and [string]::IsNullOrEmpty($env:ENERGY_SUITE_SIGNING_PASSWORD)) { Stop-Package '-Sign needs the password in the environment variable ENERGY_SUITE_SIGNING_PASSWORD.' 5 }

# Step 1: native code.
& (Join-Path $PSScriptRoot 'Build-Native.ps1') -Configuration Release -Version $Version
if ($LASTEXITCODE -ne 0) { Stop-Package "Build-Native.ps1 failed (exit $LASTEXITCODE)." }

# The web installer is built by the native build next to ews-service.exe. Without -SkipWebSetup it must be there.
if (-not $SkipWebSetup -and -not (Test-Path -LiteralPath $webSetupSource)) {
    Stop-Package "EcoWattSense-Web-Setup.exe was not built ($webSetupSource). Build it, or run again with -SkipWebSetup." 4
}

# Step 2: a clean staging folder.
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage -Force | Out-Null

# Step 3: C# programs. Setup comes last so that a missing Setup project is reported after the rest is proven to build.
Publish-Project $clientProject
Publish-Project $calibratorProject
if (-not (Test-Path -LiteralPath $setupProject)) {
    Stop-Package "Setup project not found: $setupProject. It is written separately; add it and run this script again." 3
}
Publish-Project $setupProject

# Step 4: native programs.
Copy-Item -LiteralPath (Join-Path $nativeOut 'ews-service.exe') -Destination $stage -Force
Copy-Item -LiteralPath (Join-Path $nativeOut 'ews-scan.exe') -Destination $stage -Force

# Step 5: the folder must hold the five programs.
$missing = $requiredFiles | Where-Object { -not (Test-Path -LiteralPath (Join-Path $stage $_)) }
if ($missing) { Stop-Package "the package folder lacks: $($missing -join ', ')." }

# Step 5b: the README of the suite goes next to Setup.exe.
Copy-Item -LiteralPath (Join-Path $suite 'README.md') -Destination $stage -Force

# Step 5c: signing, before the ZIP and the sums, so that they describe the signed files.
$signedFiles = $ownFiles | ForEach-Object { Join-Path $stage $_ } | Where-Object { Test-Path -LiteralPath $_ }
if ($Sign) {
    try { Invoke-SigningRun -Files $signedFiles -Pfx $PfxPath -PasswordText $env:ENERGY_SUITE_SIGNING_PASSWORD -TimestampUrl $timestampUrl }
    catch { Stop-Package "signing failed: $($_.Exception.Message)" 5 }
    Write-Host "Signed $(@($signedFiles).Count) files."
}

# Step 6: ZIP and checksum.
if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
Compress-Archive -Path $stage -DestinationPath $zipPath -CompressionLevel Optimal
$hash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
Set-Content -LiteralPath "$zipPath.sha256" -Value "$hash  $(Split-Path -Leaf $zipPath)" -Encoding ascii
$zipSize = (Get-Item -LiteralPath $zipPath).Length

# Step 7: the web installer goes next to the ZIP, with its own checksum.
$webSetupBuilt = Test-Path -LiteralPath $webSetupSource
if ($webSetupBuilt) {
    $webSetupPath = Join-Path $OutDir 'EcoWattSense-Web-Setup.exe'
    Copy-Item -LiteralPath $webSetupSource -Destination $webSetupPath -Force
    if ($Sign) {
        try { Invoke-SigningRun -Files @($webSetupPath) -Pfx $PfxPath -PasswordText $env:ENERGY_SUITE_SIGNING_PASSWORD -TimestampUrl $timestampUrl }
        catch { Stop-Package "signing the web installer failed: $($_.Exception.Message)" 5 }
    }
    $webHash = (Get-FileHash -LiteralPath $webSetupPath -Algorithm SHA256).Hash.ToLowerInvariant()
    Set-Content -LiteralPath "$webSetupPath.sha256" -Value "$webHash  EcoWattSense-Web-Setup.exe" -Encoding ascii
}

# Step 8: latest.json, the file clients read. The ZIP is published under the release tag.
$zipUrl = "https://github.com/GCU-Sector-12/ecowattsense/releases/download/$ReleaseTag/$(Split-Path -Leaf $zipPath)"
$latest = [ordered]@{ version = $Version; zip_url = $zipUrl; sha256 = $hash; size = $zipSize; server_url = $ServerUrl }
$latestPath = Join-Path $OutDir 'latest.json'
Set-Content -LiteralPath $latestPath -Value ($latest | ConvertTo-Json -Compress) -Encoding ascii

$sizeMb = [math]::Round($zipSize / 1MB, 1)
Write-Host "Package: $zipPath ($sizeMb MB)"
Write-Host "SHA256: $hash"
Write-Host "latest.json: $latestPath"
if (-not $webSetupBuilt) { Write-Host 'Web installer: skipped (-SkipWebSetup).' }
exit 0
