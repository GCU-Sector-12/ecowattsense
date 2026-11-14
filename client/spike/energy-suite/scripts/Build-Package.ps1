# Build-Package.ps1: builds the release package of the energy suite.
# Who uses it: the person who makes a release. Nothing is signed and nothing is installed.
# How to run: pwsh -NoProfile -File Build-Package.ps1 [-Version 0.1.0] [-OutDir <folder>]
# Result: <OutDir>\EcoWattSense\ (all five programs, one shared .NET runtime),
#         <OutDir>\EcoWattSense-<Version>-win-x64.zip and <OutDir>\EcoWattSense-<Version>-win-x64.zip.sha256.
# Exit codes: 0 package made; 1 a step failed or the folder is incomplete;
#             3 the Setup project is missing (it is written separately; the script does not create it).
param(
    [string] $Version = '0.1.0',
    [string] $OutDir = ''
)

$ErrorActionPreference = 'Stop'
$suite = Split-Path -Parent $PSScriptRoot
$repo = (Resolve-Path (Join-Path $suite '..\..\..')).Path
if ($OutDir -eq '') { $OutDir = Join-Path $repo 'out\package' }

$clientProject = Join-Path $repo 'client\windows\EcoWattSense.Client.Gui\EcoWattSense.Client.Gui.csproj'
$calibratorProject = Join-Path $repo 'client\spike\energy-calibrator\EnergyCalibrator.csproj'
$setupProject = Join-Path $suite 'setup\EcoWattSense.Setup\EcoWattSense.Setup.csproj'
$nativeOut = Join-Path $suite 'native\out\Release'
$stage = Join-Path $OutDir 'EcoWattSense'
$zipPath = Join-Path $OutDir "EcoWattSense-$Version-win-x64.zip"
$requiredFiles = 'ews-service.exe', 'ews-scan.exe', 'EcoWattSense.Client.Gui.exe', 'EnergyCalibrator.exe', 'Setup.exe'

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
    & dotnet publish $Project -c Release -r win-x64 --self-contained true -p:Version=$Version -p:DebugType=none -o $stage --nologo -v:minimal
    if ($LASTEXITCODE -ne 0) { Stop-Package "dotnet publish failed for $Project (exit $LASTEXITCODE)." }
}

# Step 1: native code.
& (Join-Path $PSScriptRoot 'Build-Native.ps1') -Configuration Release
if ($LASTEXITCODE -ne 0) { Stop-Package "Build-Native.ps1 failed (exit $LASTEXITCODE)." }

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

# Step 6: ZIP and checksum.
if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
Compress-Archive -Path $stage -DestinationPath $zipPath -CompressionLevel Optimal
$hash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
Set-Content -LiteralPath "$zipPath.sha256" -Value "$hash  $(Split-Path -Leaf $zipPath)" -Encoding ascii

$sizeMb = [math]::Round((Get-Item -LiteralPath $zipPath).Length / 1MB, 1)
Write-Host "Package: $zipPath ($sizeMb MB)"
Write-Host "SHA256: $hash"
exit 0
