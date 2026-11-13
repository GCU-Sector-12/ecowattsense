# Test-Native.ps1: builds the native solution in Release, runs the tests, then the nesting check.
# Who uses it: the author of the native code and the main session, as the one gate for native changes.
# How to run: pwsh -NoProfile -File Test-Native.ps1
# Output: the exit codes of the build, the test program and the nesting check, and the failed-check count.
# Exit code: 0 only when all three exit codes are 0.
$ErrorActionPreference = 'Stop'
$native = (Resolve-Path (Join-Path $PSScriptRoot '..\native')).Path
$nesting = Join-Path $PSScriptRoot '..\..\energy-calibrator\Check-Nesting.ps1'

& (Join-Path $PSScriptRoot 'Build-Native.ps1') -Configuration Release
$buildCode = $LASTEXITCODE

# A failed build leaves the test code at 1, so the gate cannot pass without a test run.
$testCode = 1
$testExe = Join-Path $native 'out\Release\ews-core-tests.exe'
if ($buildCode -eq 0 -and (Test-Path -LiteralPath $testExe)) {
    & $testExe
    $testCode = $LASTEXITCODE
}

& pwsh -NoProfile -File $nesting -Path $native
$nestingCode = $LASTEXITCODE

Write-Host "build exit code: $buildCode"
Write-Host "tests exit code (= failed checks): $testCode"
Write-Host "nesting check exit code: $nestingCode"
if ($buildCode -eq 0 -and $testCode -eq 0 -and $nestingCode -eq 0) { exit 0 }
exit 1
