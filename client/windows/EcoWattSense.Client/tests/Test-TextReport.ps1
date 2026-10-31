$ErrorActionPreference = 'Stop'
Add-Type -Path @(
    (Join-Path $PSScriptRoot '../Monitoring/StateClassifier.cs'),
    (Join-Path $PSScriptRoot '../Reporting/DeviceReport.cs'),
    (Join-Path $PSScriptRoot '../Reporting/ReportJson.cs'),
    (Join-Path $PSScriptRoot '../Reporting/TextReportWriter.cs')
)
$directory = Join-Path ([IO.Path]::GetTempPath()) ('EcoWattSense-TextTest-' + [guid]::NewGuid().ToString('N'))
$writer = [EcoWattSense.Client.Reporting.TextReportWriter]::new($directory)
try {
    $id = [guid]::NewGuid()
    $timestamp = [datetime]::new(2026, 10, 25, 12, 0, 0, [DateTimeKind]::Utc)
    $report = [EcoWattSense.Client.Reporting.DeviceReport]::new($id, $timestamp, 15,
        [EcoWattSense.Client.Monitoring.ActivityState]::Idle, 650, 37.5, $true, $false, 'laptop')
    $writer.Save($report)
    $text = Get-Content -Raw -LiteralPath $writer.FilePath
    foreach ($expected in @($id.ToString(), 'Idle (1)', '650 seconds', '37.50%', 'Mains', '2026-10-25 12:00:00', 'not server acceptance')) {
        if (!$text.Contains($expected)) { throw "Report missing: $expected" }
    }
    $json = $text.Substring($text.IndexOf('{')) | ConvertFrom-Json
    if ($json.device_id -ne $id.ToString() -or $json.cpu_percent -ne 37.5 -or !$json.screen_locked -or $json.on_battery) { throw 'JSON differs from measurements' }
    $next = [EcoWattSense.Client.Reporting.DeviceReport]::new($id, $timestamp.AddSeconds(15), 15,
        [EcoWattSense.Client.Monitoring.ActivityState]::Active, 0, 0, $false, $true, 'laptop')
    $writer.Save($next)
    $text = Get-Content -Raw -LiteralPath $writer.FilePath
    if (!$text.Contains('Active (2)') -or !$text.Contains('Battery') -or $text.Contains('Idle (1)')) { throw 'Latest report was not replaced' }
    Write-Output 'Passed text report checks: summary, JSON values and latest-file replacement.'
} finally {
    if (Test-Path -LiteralPath $writer.FilePath) { Remove-Item -LiteralPath $writer.FilePath -Force }
    if (Test-Path -LiteralPath $directory) { Remove-Item -LiteralPath $directory } # Empty directory only.
}
