# Verify-ReleasePackage.ps1: checks the Windows release inventory and Authenticode signatures.
# GitHub Actions also checks the inventory of the unsigned package on pull requests.
param(
    [Parameter(Mandatory)] [string] $PackageDirectory,
    [Parameter(Mandatory)] [ValidatePattern('^\d+(\.\d+){0,3}$')] [string] $Version,
    [switch] $InventoryOnly
)

$ErrorActionPreference = 'Stop'
$binaries = Import-PowerShellDataFile -LiteralPath (Join-Path $PSScriptRoot 'PackageBinaries.psd1')
$pin = 'F53EC709CE14C833AECF191E11DAB909446B6CBA'

<#
.SYNOPSIS Checks required and approved executable paths below one package root.
.PARAMETER Root The staged or extracted package directory.
.OUTPUTS The approved executable paths. Throws for a missing or unexpected file.
#>
function Get-PackageBinaries([string] $Root) {
    $paths = @(Get-ChildItem -LiteralPath $Root -File -Recurse -Force |
        Where-Object Extension -in '.exe', '.dll' |
        ForEach-Object { [IO.Path]::GetRelativePath($Root, $_.FullName).Replace('\', '/') })
    $missing = @($binaries.Own | Where-Object { $_ -notin $paths })
    $allowed = $binaries.Own + $binaries.Microsoft
    $unexpected = @($paths | Where-Object { $_ -notin $allowed })
    if ($missing.Count) { throw "Missing EcoWattSense binaries in $Root : $($missing -join ', ')" }
    if ($unexpected.Count) { throw "Unexpected binaries in $Root : $($unexpected -join ', ')" }
    return $paths
}

<#
.SYNOPSIS Checks one binary for its expected publisher and signature status.
.PARAMETER File The full path of the file.
.PARAMETER Own True for a project binary, false for an approved Microsoft binary.
.OUTPUTS An error string on failure. The updater's WinVerifyTrust check handles the team certificate.
#>
function Get-SignatureProblem([string] $File, [bool] $Own) {
    if ($Own) { return Get-OwnSignatureProblem $File }
    $signature = Get-AuthenticodeSignature -LiteralPath $File
    $signer = $signature.SignerCertificate
    if ($signature.Status -eq 'Valid' -and $signer.Subject -match '(?:^|,\s*)O=Microsoft Corporation(?:,|$)') { return }
    return "$File ($($signature.Status), signer $($signer.Thumbprint))"
}

<#
.SYNOPSIS Verifies a project binary with the same Windows trust and pin rules as the updater.
.PARAMETER File The full path of the file.
.OUTPUTS An error string on failure. The checker refuses unsigned or changed files.
#>
function Get-OwnSignatureProblem([string] $File) {
    $verdict = [EcoWattSense.Client.Gui.Update.SignatureChecker]::Check($File, $pin)
    if ($verdict -eq 'Start') { return }
    return "$File (team signature or pin check failed)"
}

<#
.SYNOPSIS Reports a binary whose contents differ between staging and the ZIP.
.PARAMETER Stage Full path of the staged package root.
.PARAMETER Extracted Full path of the extracted package root.
.PARAMETER Relative The binary path relative to either root.
.OUTPUTS The relative path on a hash mismatch. A read failure stops the check.
#>
function Get-BinaryHashDifference([string] $Stage, [string] $Extracted, [string] $Relative) {
    $stagedHash = (Get-FileHash -LiteralPath (Join-Path $Stage $Relative)).Hash
    $archivedHash = (Get-FileHash -LiteralPath (Join-Path $Extracted $Relative)).Hash
    if ($stagedHash -ne $archivedHash) { return $Relative }
}

<#
.SYNOPSIS Checks inventory and compares staged binaries with the contents of the ZIP.
.PARAMETER Stage Full path of the staged package root.
.PARAMETER Extracted Full path of the extracted package root.
.PARAMETER WebSetup The standalone web installer next to the ZIP.
.OUTPUTS The checked ZIP paths. Throws if the inventory or any hash differs.
#>
function Compare-PackageBinaries([string] $Stage, [string] $Extracted, [string] $WebSetup) {
    $stagePaths = @(Get-PackageBinaries $Stage)
    $zipPaths = @(Get-PackageBinaries $Extracted)
    $difference = @(Compare-Object $stagePaths $zipPaths)
    if ($difference.Count) { throw "Staged and ZIP binary paths differ: $($difference.InputObject -join ', ')" }
    if (-not (Test-Path -LiteralPath $WebSetup -PathType Leaf)) { throw "Missing web installer: $WebSetup" }

    $changed = foreach ($relative in $zipPaths) {
        Get-BinaryHashDifference $Stage $Extracted $relative
    }
    if ($changed) { throw "Staged and ZIP binaries differ: $($changed -join ', ')" }
    return $zipPaths
}

<#
.SYNOPSIS Checks the extracted package and the web installer.
.PARAMETER Stage The staged package root.
.PARAMETER Extracted The extracted package root.
.PARAMETER WebSetup The standalone web installer next to the ZIP.
.OUTPUTS A summary. Throws when the inventory or a required signature fails.
#>
function Confirm-Package([string] $Stage, [string] $Extracted, [string] $WebSetup) {
    $zipPaths = @(Compare-PackageBinaries $Stage $Extracted $WebSetup)
    if ($InventoryOnly) {
        Write-Host "Package inventory passed ($($zipPaths.Count) binaries and the web installer)."
        return
    }

    $problems = @(
        Get-SignatureProblem $WebSetup $true
        foreach ($relative in $zipPaths) {
            Get-SignatureProblem (Join-Path $Extracted $relative) ($relative -in $binaries.Own)
        }
    )
    $newLine = [Environment]::NewLine
    if ($problems.Count) { throw ("Package signature verification failed:" + $newLine + ($problems -join $newLine)) }
    Write-Host "Verified $($zipPaths.Count) signatures in the ZIP and the web installer."
}

<#
.SYNOPSIS Removes the verifier's private extraction folder when it exists.
.PARAMETER Directory The temporary directory created for this run.
.OUTPUTS Nothing. Reports an error if cleanup fails.
#>
function Remove-VerificationFolder([string] $Directory) {
    if (-not (Test-Path -LiteralPath $Directory)) { return }
    Remove-Item -LiteralPath $Directory -Recurse -Force
}

$stage = (Resolve-Path -LiteralPath (Join-Path $PackageDirectory 'EcoWattSense')).Path
$zip = Join-Path $PackageDirectory "EcoWattSense-$Version-win-x64.zip"
$webSetup = Join-Path $PackageDirectory 'EcoWattSense-Web-Setup.exe'
$certificatePath = Join-Path $PSScriptRoot '../signing/EcoWattSense-CodeSigning.cer'
$certificate = [System.Security.Cryptography.X509Certificates.X509Certificate2]::new($certificatePath)
$certificatePin = $certificate.Thumbprint
$certificate.Dispose()
if ($certificatePin -ne $pin) { throw 'The checked-in certificate does not match the release pin.' }

if (-not $InventoryOnly) {
    $update = Join-Path $PSScriptRoot '../EcoWattSense.Client.Gui/Update'
    Add-Type -Path (Join-Path $update 'SigningPin.cs'), (Join-Path $update 'SignatureRules.cs'), (Join-Path $update 'SignatureChecker.cs')
}
if (-not $InventoryOnly -and [EcoWattSense.Client.Gui.Update.SigningPin]::Thumbprint -ne $pin) {
    throw 'The updater pin does not match the release pin.'
}

$work = Join-Path ([IO.Path]::GetTempPath()) ("ews-package-verify-" + [guid]::NewGuid())
try {
    Expand-Archive -LiteralPath $zip -DestinationPath $work
    Confirm-Package $stage (Join-Path $work 'EcoWattSense') $webSetup
}
finally {
    Remove-VerificationFolder $work
}
