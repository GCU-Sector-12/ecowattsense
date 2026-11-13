# Build-Native.ps1: builds the native solution (ews-core, ews-core-tests) for x64.
# Who uses it: the author of the native code, Test-Native.ps1, and the package script.
# How to run: pwsh -NoProfile -File Build-Native.ps1 [-Configuration Release|Debug]
# Exit code: MSBuild's exit code (0 = built with no errors; warnings are errors in this solution).
#            2 when vswhere.exe or MSBuild.exe cannot be found.
param(
    [ValidateSet('Release', 'Debug')]
    [string] $Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$solution = Join-Path $PSScriptRoot '..\native\EwsNative.sln'

if (-not (Test-Path -LiteralPath $vswhere)) {
    Write-Error "vswhere.exe not found at $vswhere. Install Visual Studio 2022 with the C++ desktop workload."
    exit 2
}

$msbuild = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild -or -not (Test-Path -LiteralPath $msbuild)) {
    Write-Error 'MSBuild.exe with the x64 C++ tools was not found by vswhere.'
    exit 2
}

# /nodeReuse:false: otherwise MSBuild worker processes stay alive after the build and keep the build folder locked.
& $msbuild $solution /nologo /m /nodeReuse:false /v:minimal /p:Configuration=$Configuration /p:Platform=x64
exit $LASTEXITCODE
