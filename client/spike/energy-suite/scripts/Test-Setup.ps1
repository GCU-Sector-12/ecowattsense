<#
.SYNOPSIS
Builds and runs the console tests of Setup.exe (setup\tests\SetupTests).

.DESCRIPTION
Uses fakes only: it never installs anything and never touches the real service, Program Files or ProgramData.
Prints every check. The exit code is the number of failed checks, or 1 when the build fails.

.EXAMPLE
pwsh -File .\Test-Setup.ps1
#>
$ErrorActionPreference = 'Stop'
$project = Join-Path $PSScriptRoot '..\setup\tests\SetupTests\SetupTests.csproj'
dotnet build $project -nodeReuse:false --nologo -v q
if ($LASTEXITCODE -ne 0) { exit 1 }
dotnet run --project $project --no-build
exit $LASTEXITCODE
