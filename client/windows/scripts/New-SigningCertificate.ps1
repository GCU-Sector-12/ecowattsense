# New-SigningCertificate.ps1: creates the self-signed code signing certificate of the energy suite.
# Who uses it: Darek, ONCE, as himself, on his own laptop. Nobody else runs it (see docs\SIGNING.md).
# How to run: pwsh -NoProfile -File New-SigningCertificate.ps1 [-PfxPath <file.pfx>]
# What it does:
#   1. creates the certificate in Cert:\CurrentUser\My (RSA 3072, SHA256, valid 2 years, code signing);
#   2. exports the PUBLIC part to signing\EcoWattSense-CodeSigning.cer in the suite (this file is committed);
#   3. asks for a password twice (hidden) and exports the PRIVATE key to a .pfx. The default place is the Documents folder.
#      A .pfx path inside the git repository is refused, so the private key can never be committed by accident.
# It never prints the password and never prints the .pfx as base64. The password is never a parameter.
# Exit codes: 0 done; 1 the passwords differ or are empty; 2 the .pfx path is inside the repository or the file exists.
param(
    [string] $PfxPath = ''
)

$ErrorActionPreference = 'Stop'
$suite = Split-Path -Parent $PSScriptRoot
$subject = 'CN=EcoWattSense Code Signing, O=GCU Sector 12'
$cerPath = Join-Path $suite 'signing\EcoWattSense-CodeSigning.cer'
if ($PfxPath -eq '') { $PfxPath = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'EcoWattSense-CodeSigning.pfx' }

<#
.SYNOPSIS Stops the script with a message and an exit code.
.PARAMETER Message What went wrong, in plain words.
.PARAMETER Code The exit code.
#>
function Stop-Script {
    param([string] $Message, [int] $Code)
    [Console]::Error.WriteLine("New-SigningCertificate: $Message")
    exit $Code
}

<#
.SYNOPSIS Tells whether a path lies inside the git repository that holds this script.
.PARAMETER Path The path to test (it need not exist yet).
.OUTPUTS $true when the path is inside the repository.
#>
function Test-InsideRepository {
    param([string] $Path)
    $top = (& git -C $suite rev-parse --show-toplevel 2>$null)
    if ($LASTEXITCODE -ne 0 -or -not $top) { return $false }
    $root = [IO.Path]::GetFullPath($top).TrimEnd('\', '/') + '\'
    $full = [IO.Path]::GetFullPath($Path)
    return $full.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)
}

<#
.SYNOPSIS Reads a password twice from the console, hidden.
.OUTPUTS The password as a SecureString, or $null when the two entries differ or are empty.
#>
function Read-NewPassword {
    $first = Read-Host -AsSecureString -Prompt 'Password for the .pfx'
    $second = Read-Host -AsSecureString -Prompt 'Repeat the password'
    $a = [System.Net.NetworkCredential]::new('', $first).Password
    $b = [System.Net.NetworkCredential]::new('', $second).Password
    $same = ($a -ne '') -and ($a -ceq $b)
    if (-not $same) { return $null }
    return $first
}

if (Test-InsideRepository $PfxPath) { Stop-Script "the .pfx path is inside the repository: $PfxPath. Choose a folder outside it." 2 }
if (Test-Path -LiteralPath $PfxPath) { Stop-Script "the file exists already: $PfxPath. Move it or choose another path." 2 }

$password = Read-NewPassword
if (-not $password) { Stop-Script 'the passwords are empty or differ. Nothing was created.' 1 }

$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject $subject -KeyAlgorithm RSA -KeyLength 3072 -HashAlgorithm SHA256 `
    -KeyExportPolicy Exportable -NotAfter (Get-Date).AddYears(2) -CertStoreLocation 'Cert:\CurrentUser\My'

New-Item -ItemType Directory -Path (Split-Path -Parent $cerPath) -Force | Out-Null
Export-Certificate -Cert $cert -FilePath $cerPath -Type CERT | Out-Null
New-Item -ItemType Directory -Path (Split-Path -Parent $PfxPath) -Force | Out-Null
Export-PfxCertificate -Cert $cert -FilePath $PfxPath -Password $password | Out-Null

Write-Host "Thumbprint: $($cert.Thumbprint)"
Write-Host "Valid until: $($cert.NotAfter.ToString('yyyy-MM-dd'))"
Write-Host "Public certificate (commit it): $cerPath"
Write-Host "Private key (.pfx, keep it secret): $PfxPath"
Write-Host ''
Write-Host 'Next steps:'
Write-Host '  1. Store the .pfx file and its password in the password manager of the team.'
Write-Host '  2. Add the GitHub secrets (the password is asked by gh; the base64 is made by the shell and not shown):'
Write-Host '       gh secret set ENERGY_SUITE_SIGNING_PASSWORD'
Write-Host '       [Convert]::ToBase64String([IO.File]::ReadAllBytes("<pfx path>")) | gh secret set ENERGY_SUITE_SIGNING_PFX'
Write-Host '  3. Pin the thumbprint above in the programs (see docs\SIGNING.md).'
Write-Host "  4. If you want no local signing, delete the certificate from your store: Remove-Item Cert:\CurrentUser\My\$($cert.Thumbprint)"
exit 0
