$ErrorActionPreference = 'Stop'
Add-Type -Path @(
    (Join-Path $PSScriptRoot '../Monitoring/StateClassifier.cs'),
    (Join-Path $PSScriptRoot '../Reporting/ActivityHistoryWriter.cs')
)
$directory = Join-Path ([IO.Path]::GetTempPath()) ('EcoWattSense-HistoryTest-' + [guid]::NewGuid().ToString('N'))
try {
    $writer = [EcoWattSense.Client.Reporting.ActivityHistoryWriter]::new($directory)
    $id = [guid]::NewGuid()
    $time = [datetime]::UtcNow
    $writer.Save($id, 'laptop', $time, 0, $null, $null, $null, $null)
    $first = @(Get-Content -Raw $writer.FilePath | ConvertFrom-Json)
    if ($first.Count -ne 1 -or $null -ne $first[0].cpu_percent -or $null -ne $first[0].screen_locked) { throw 'First/unknown observation failed' }
    $writer = [EcoWattSense.Client.Reporting.ActivityHistoryWriter]::new($directory)
    $writer.Save($id, 'laptop', $time.AddSeconds(5), 600.5, 25,
        [EcoWattSense.Client.Monitoring.ActivityState]::Idle, $false, $true)
    $items = @(Get-Content -Raw $writer.FilePath | ConvertFrom-Json)
    if ($items.Count -ne 2 -or $items[0].device_id -ne $id.ToString() -or
        $items[1].state -ne 1 -or $items[1].state_name -ne 'Idle' -or
        $items[1].idle_seconds -ne 600.5 -or $items[1].cpu_percent -ne 25 -or
        $items[1].screen_locked -ne $false -or $items[1].on_battery -ne $true) { throw 'Append/restart/value check failed' }
    Set-Content -LiteralPath $writer.FilePath -Value 'broken history'
    $failed = $false
    try { $writer.Save($id, 'laptop', $time, 0, 0, $null, $false, $false) } catch { $failed = $true }
    if (!$failed -or !(Get-Content -Raw $writer.FilePath).Contains('broken history')) { throw 'Damaged history was overwritten' }
    Write-Output 'Passed activity history checks: every observation, nulls, values, restart append and invalid-file preservation.'
} finally {
    $resolved = [IO.Path]::GetFullPath($directory)
    $parent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar)
    if ([IO.Path]::GetDirectoryName($resolved) -ne $parent -or ![IO.Path]::GetFileName($resolved).StartsWith('EcoWattSense-HistoryTest-')) { throw 'Unsafe cleanup' }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
