$ErrorActionPreference = 'Stop'
Add-Type -Path @(
    (Join-Path $PSScriptRoot '../Monitoring/StateClassifier.cs'),
    (Join-Path $PSScriptRoot '../Reporting/DeviceReport.cs'),
    (Join-Path $PSScriptRoot '../Reporting/ReportJson.cs'),
    (Join-Path $PSScriptRoot '../Reporting/JsonReportWriter.cs')
)
$directory = Join-Path ([IO.Path]::GetTempPath()) ('EcoWattSense-JsonTest-' + [guid]::NewGuid().ToString('N'))
try {
    $writer = [EcoWattSense.Client.Reporting.JsonReportWriter]::new($directory)
    $id = [guid]::NewGuid()
    $time = [datetime]::new(2026, 10, 25, 23, 59, 50, [DateTimeKind]::Utc)
    $first = [EcoWattSense.Client.Reporting.DeviceReport]::new($id, $time, 15,
        [EcoWattSense.Client.Monitoring.ActivityState]::Active, 0, 12.5, $false, $true, 'laptop')
    $firstPath = $writer.Save($first)
    $original = [IO.File]::ReadAllText($firstPath)
    $second = [EcoWattSense.Client.Reporting.DeviceReport]::new($id, $time.AddSeconds(15), 15,
        [EcoWattSense.Client.Monitoring.ActivityState]::Idle, 600, 25, $true, $false, 'laptop')
    # New writer simulates a restart; history must survive.
    $writer = [EcoWattSense.Client.Reporting.JsonReportWriter]::new($directory)
    $secondPath = $writer.Save($second)
    if ([IO.File]::ReadAllText($firstPath) -cne $original) { throw 'History was changed' }
    if ($firstPath -eq $secondPath -or !(Test-Path -LiteralPath $secondPath)) { throw 'Second interval missing' }
    if ($firstPath -notmatch '2026-10-25' -or $secondPath -notmatch '2026-10-26') { throw 'UTC date folders incorrect' }
    $expected = [EcoWattSense.Client.Reporting.ReportJson]::Serialize($second) | ConvertFrom-Json -AsHashtable
    $actual = Get-Content -Raw -LiteralPath $writer.LatestPath | ConvertFrom-Json -AsHashtable
    if ($actual.Count -ne 10) { throw 'Expected exactly ten fields' }
    foreach ($key in $expected.Keys) {
        if ($actual[$key] -cne $expected[$key]) { throw "Mismatch: $key" }
    }
    $rejected = $false
    try { $null = $writer.Save($first) } catch { $rejected = $true }
    if (!$rejected -or [IO.File]::ReadAllText($firstPath) -cne $original) { throw 'Duplicate overwrote history' }
    Write-Output 'Passed JSON file checks: schema fields/values, history, restart, UTC date rollover, latest copy and overwrite protection.'
} finally {
    $resolved = [IO.Path]::GetFullPath($directory)
    $parent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar)
    if ([IO.Path]::GetDirectoryName($resolved) -ne $parent -or ![IO.Path]::GetFileName($resolved).StartsWith('EcoWattSense-JsonTest-')) { throw 'Unsafe cleanup' }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
