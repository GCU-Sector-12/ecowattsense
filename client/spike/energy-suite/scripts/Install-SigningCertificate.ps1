# Install-SigningCertificate.ps1: makes a laptop trust the EcoWattSense code signing certificate.
# Who uses it: every team member, once per laptop, in an ADMINISTRATOR PowerShell.
# How to run: pwsh -NoProfile -File Install-SigningCertificate.ps1 [-CerPath <file.cer>] [-Remove]
# What it does: imports the PUBLIC certificate (default: signing\EcoWattSense-CodeSigning.cer in the suite) into
#   Cert:\LocalMachine\Root and Cert:\LocalMachine\TrustedPublisher. It shows the subject and the thumbprint first and asks Y/N.
#   -Remove takes the certificate out of both stores again (after a leak or when a new one replaces it).
# Exit codes: 0 done; 1 not an administrator, or the .cer is missing; 2 the user answered no.
param(
    [string] $CerPath = '',
    [switch] $Remove
)

$ErrorActionPreference = 'Stop'
$suite = Split-Path -Parent $PSScriptRoot
if ($CerPath -eq '') { $CerPath = Join-Path $suite 'signing\EcoWattSense-CodeSigning.cer' }
$stores = 'Root', 'TrustedPublisher'

<#
.SYNOPSIS Stops the script with a message and an exit code.
.PARAMETER Message What went wrong, in plain words.
.PARAMETER Code The exit code.
#>
function Stop-Script {
    param([string] $Message, [int] $Code)
    [Console]::Error.WriteLine("Install-SigningCertificate: $Message")
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

if (-not (Test-Administrator)) { Stop-Script 'run this in an administrator PowerShell.' 1 }
if (-not (Test-Path -LiteralPath $CerPath)) { Stop-Script "certificate file not found: $CerPath" 1 }

$cert = [System.Security.Cryptography.X509Certificates.X509Certificate2]::new($CerPath)
$action = if ($Remove) { 'REMOVE from' } else { 'ADD to' }
Write-Host "Subject:    $($cert.Subject)"
Write-Host "Thumbprint: $($cert.Thumbprint)"
Write-Host "This will $action the machine stores: $($stores -join ', ')."
$answer = Read-Host 'Continue? (Y/N)'
if ($answer -notmatch '^[Yy]$') { Stop-Script 'cancelled, nothing changed.' 2 }

foreach ($store in $stores) {
    $location = "Cert:\LocalMachine\$store"
    if ($Remove) { Get-ChildItem -Path $location | Where-Object { $_.Thumbprint -eq $cert.Thumbprint } | Remove-Item -Force }
    else { Import-Certificate -FilePath $CerPath -CertStoreLocation $location | Out-Null }
    Write-Host "Done: $location"
}
exit 0
