# Trust-EcoWattSense.ps1: makes this laptop trust the EcoWattSense team code signing certificate with one command.
# Who uses it: team members who have NO clone of the repository (with a clone use Install-SigningCertificate.ps1).
# How to run: powershell -NoProfile -ExecutionPolicy Bypass -File Trust-EcoWattSense.ps1 [-Remove] [-WhatIf]
#   Works in Windows PowerShell 5.1 and in pwsh 7. Without administrator rights it restarts itself elevated (UAC prompt).
# What it does: downloads the PUBLIC certificate from GitHub (or reads -CertificatePath, used by tests), checks that its
#   thumbprint is exactly the pinned one, then imports it into Cert:\LocalMachine\Root and Cert:\LocalMachine\TrustedPublisher.
#   -Remove takes it out of both stores. -WhatIf prints the plan and changes nothing.
# Why the pin: the download is only trusted when the thumbprint matches, so a changed or swapped file is refused.
# Exit codes: 0 done (or -WhatIf plan shown); 1 not elevated or UAC cancelled; 2 certificate not obtained or wrong
#   thumbprint (no store touched); 3 a store does not have the expected state afterwards.
# -NoElevate is a hidden switch for tests: it prints the elevated restart command and exits 1 instead of starting it.
param(
    [switch] $Remove,
    [switch] $WhatIf,
    [string] $CertificatePath = '',
    [switch] $NoElevate
)

$ErrorActionPreference = 'Stop'
$pinnedThumbprint = 'F53EC709CE14C833AECF191E11DAB909446B6CBA'
$certificateUrl = 'https://raw.githubusercontent.com/GCU-Sector-12/ecowattsense/EWS-energy-suite/client/spike/energy-suite/signing/EcoWattSense-CodeSigning.cer'
$scriptUrl = 'https://raw.githubusercontent.com/GCU-Sector-12/ecowattsense/EWS-energy-suite/client/spike/energy-suite/scripts/Trust-EcoWattSense.ps1'
$stores = 'Root', 'TrustedPublisher'
$script:tempFile = ''

<#
.SYNOPSIS Deletes the temp certificate file if one was made.
#>
function Remove-TempFile {
    if ($script:tempFile -eq '') { return }
    Remove-Item -LiteralPath $script:tempFile -Force -ErrorAction SilentlyContinue
    $script:tempFile = ''
}

<#
.SYNOPSIS Cleans the temp file, prints a message and ends the script.
.PARAMETER Message What happened, in plain words.
.PARAMETER Code The exit code.
#>
function Stop-Script {
    param([string] $Message, [int] $Code)
    Remove-TempFile
    [Console]::Error.WriteLine("Trust-EcoWattSense: $Message")
    exit $Code
}

<#
.SYNOPSIS Tells whether the current PowerShell runs as administrator.
.OUTPUTS $true for an administrator.
#>
function Test-Administrator {
    $principal = [Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

<#
.SYNOPSIS Downloads the public certificate into a new temp file.
.OUTPUTS The temp file path. Stops the script with exit 2 when the download fails.
#>
function Get-CertificateFile {
    $script:tempFile = Join-Path ([IO.Path]::GetTempPath()) ('EcoWattSense-' + [guid]::NewGuid().ToString('N') + '.cer')
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    $failure = $null
    Invoke-WebRequest -UseBasicParsing -Uri $certificateUrl -OutFile $script:tempFile -ErrorVariable failure -ErrorAction SilentlyContinue
    if ($failure) { Stop-Script "could not download the certificate: $($failure[0].Exception.Message)" 2 }
    return $script:tempFile
}

<#
.SYNOPSIS Loads a .cer file and checks it carries the pinned thumbprint.
.PARAMETER Path The .cer file.
.OUTPUTS The X509Certificate2. Stops the script with exit 2 when it cannot be read or the thumbprint differs.
#>
function Read-PinnedCertificate {
    param([string] $Path)
    $failure = $null
    $cert = Get-PfxCertificate -FilePath $Path -ErrorVariable failure -ErrorAction SilentlyContinue
    if ($failure) { Stop-Script "the file is not a readable certificate: $($failure[0].Exception.Message)" 2 }
    if ($cert.Thumbprint -ne $pinnedThumbprint) { Stop-Script "WRONG thumbprint $($cert.Thumbprint), expected $pinnedThumbprint. Nothing was changed." 2 }
    return $cert
}

<#
.SYNOPSIS Starts this script again elevated and ends with its exit code, or explains what to do.
.DESCRIPTION From a file it uses Start-Process -Verb RunAs and waits. Piped (no file path) it prints the one-line command.
  A cancelled UAC prompt gives a clear message and exit 1.
#>
function Start-Elevated {
    $flag = if ($Remove) { ' -Remove' } else { '' }
    if (-not $PSCommandPath) { Stop-Script "run this in an administrator PowerShell: irm $scriptUrl -OutFile `$env:TEMP\Trust-EcoWattSense.ps1; & `$env:TEMP\Trust-EcoWattSense.ps1$flag" 1 }
    $arguments = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`"$flag"
    if ($NoElevate) { Stop-Script "would restart elevated: Start-Process powershell.exe -Verb RunAs -ArgumentList '$arguments' -Wait" 1 }
    $failure = $null
    $process = Start-Process powershell.exe -Verb RunAs -ArgumentList $arguments -Wait -PassThru -ErrorVariable failure -ErrorAction SilentlyContinue
    if ($failure) { Stop-Script 'the administrator prompt was cancelled or refused. Nothing was changed.' 1 }
    exit $process.ExitCode
}

<#
.SYNOPSIS Imports the certificate into one machine store.
.PARAMETER Path The .cer file.
.PARAMETER Store Root or TrustedPublisher.
#>
function Add-ToStore {
    param([string] $Path, [string] $Store)
    Import-Certificate -FilePath $Path -CertStoreLocation "Cert:\LocalMachine\$Store" | Out-Null
}

<#
.SYNOPSIS Removes the pinned thumbprint from one machine store, if it is there.
.PARAMETER Store Root or TrustedPublisher.
#>
function Remove-FromStore {
    param([string] $Store)
    $location = "Cert:\LocalMachine\$Store\$pinnedThumbprint"
    if (-not (Test-Path -LiteralPath $location)) { return }
    Remove-Item -LiteralPath $location -Force
}

<#
.SYNOPSIS Imports or removes the certificate in one store, depending on -Remove.
.PARAMETER Path The .cer file.
.PARAMETER Store Root or TrustedPublisher.
#>
function Update-Store {
    param([string] $Path, [string] $Store)
    if ($Remove) { Remove-FromStore $Store; return }
    Add-ToStore $Path $Store
}

<#
.SYNOPSIS Checks one store has (or, with -Remove, lacks) the certificate. Stops with exit 3 otherwise.
.PARAMETER Store Root or TrustedPublisher.
#>
function Confirm-Store {
    param([string] $Store)
    $present = Test-Path -LiteralPath "Cert:\LocalMachine\$Store\$pinnedThumbprint"
    if ($present -eq [bool] $Remove) { Stop-Script "Cert:\LocalMachine\$Store does not have the expected state." 3 }
    Write-Host "OK: Cert:\LocalMachine\$Store"
}

<#
.SYNOPSIS Prints what would be done to one store (used by -WhatIf).
.PARAMETER Store Root or TrustedPublisher.
#>
function Show-Plan {
    param([string] $Store)
    $verb = if ($Remove) { 'remove from' } else { 'import into' }
    Write-Host "WhatIf: would $verb Cert:\LocalMachine\$Store"
}

$file = $CertificatePath
if ($file -eq '') { $file = Get-CertificateFile }
$cert = Read-PinnedCertificate $file
Write-Host "Subject:    $($cert.Subject)"
Write-Host "Thumbprint: $($cert.Thumbprint) (matches the pinned value)"
if ($WhatIf) {
    $stores | ForEach-Object { Show-Plan $_ }
    Write-Host 'WhatIf: nothing was changed.'
    Remove-TempFile
    exit 0
}
if (-not (Test-Administrator)) { Remove-TempFile; Start-Elevated }
$stores | ForEach-Object { Update-Store $file $_ }
Remove-TempFile
$stores | ForEach-Object { Confirm-Store $_ }
exit 0
