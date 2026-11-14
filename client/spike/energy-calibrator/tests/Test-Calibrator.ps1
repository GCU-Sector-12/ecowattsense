# Test-Calibrator.ps1: builds and runs the console tests of the EnergyCalibrator model file (schema 2).
# Who uses it: the author of the calibrator code and the main session. Run: pwsh -NoProfile -File Test-Calibrator.ps1
# What it does: dotnet build of tests\CalibratorTests (it builds the calibrator too), then runs the test program.
# Exit code: the number of failed checks (0 = all passed); 1000 when the build fails.
# The tests never read or write C:\ProgramData: they use a temporary folder. They do not start the calibrator test itself.
$ErrorActionPreference = 'Stop'
$project = Join-Path $PSScriptRoot 'CalibratorTests\CalibratorTests.csproj'
dotnet build $project -nodeReuse:false --nologo -v:minimal
if ($LASTEXITCODE -ne 0) { Write-Host 'BUILD FAILED'; exit 1000 }
$program = Join-Path $PSScriptRoot 'CalibratorTests\bin\Debug\net10.0-windows\CalibratorTests.dll'
dotnet $program
exit $LASTEXITCODE
