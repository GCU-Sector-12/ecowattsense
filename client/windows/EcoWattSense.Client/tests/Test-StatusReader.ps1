# Run with PowerShell 7 (.NET 10): pwsh -File ./tests/Test-StatusReader.ps1
# Checks status.json parsing (missing file, bad JSON, unknown words, a good file, oversize) and the card texts.
$ErrorActionPreference = 'Stop'
$viewer = Join-Path $PSScriptRoot '../../EcoWattSense.Client.Gui/Viewer'
Add-Type -Path @((Join-Path $viewer 'ViewerMode.cs'), (Join-Path $viewer 'StatusReader.cs'), (Join-Path $viewer 'ViewerText.cs'))
$reader = [EcoWattSense.Client.Gui.Viewer.StatusReader]
$text = [EcoWattSense.Client.Gui.Viewer.ViewerText]
$directory = Join-Path ([IO.Path]::GetTempPath()) ('EcoWattSense-StatusTest-' + [guid]::NewGuid().ToString('N'))
$checks = 0
try {
    $null = New-Item -ItemType Directory -Path $directory
    $path = Join-Path $directory 'status.json'

    # Missing file.
    if ($reader::Read($path).Valid) { throw 'Missing file reported valid' }
    $checks++

    # Bad JSON, a JSON array, an empty file.
    foreach ($bad in @('{ not json', '[1,2]', '', '"text"')) {
        Set-Content -LiteralPath $path -Value $bad -NoNewline
        if ($reader::Read($path).Valid) { throw "Bad content reported valid: $bad" }
        $checks++
    }

    # Oversize file is refused.
    Set-Content -LiteralPath $path -Value ('{"level":"L1","pad":"' + ('x' * 70000) + '"}') -NoNewline
    if ($reader::Read($path).Valid) { throw 'Oversize file accepted' }
    $checks++

    # Unknown level and unknown send-result words, wrong types, bad dates: valid object, fields null.
    Set-Content -LiteralPath $path -Value '{"level":"L9","last_send_result":"boom","model_generated_utc":"yesterday","last_report_utc":5}' -NoNewline
    $odd = $reader::Read($path)
    if (-not $odd.Valid -or $null -ne $odd.Level -or $null -ne $odd.SendResult -or $null -ne $odd.ModelGeneratedUtc -or $null -ne $odd.LastReportUtc) { throw 'Unknown words not rejected' }
    # PowerShell turns a null string argument into an empty string, so a real null is passed explicitly.
    if ($text::LevelWord([System.Management.Automation.Language.NullString]::Value) -ne 'Unknown') { throw 'Unknown level word wrong' }
    $checks += 2

    # A good file (the example of SPEC 5.7).
    $good = '{ "schema": 1, "service_version": "0.1.0", "level": "L2", "model_generated_utc": "2026-11-12T17:08:31Z", "last_report_utc": "2026-11-14T10:05:00Z", "last_send_utc": "2026-11-14T10:05:01Z", "last_send_result": "ok", "queued": 0 }'
    Set-Content -LiteralPath $path -Value $good -NoNewline
    $status = $reader::Read($path)
    if (-not $status.Valid -or $status.Level -ne 'L2' -or $status.SendResult -ne 'ok') { throw 'Good file fields wrong' }
    if ($status.ModelGeneratedUtc.UtcDateTime -ne [datetime]::new(2026, 11, 12, 17, 8, 31, [DateTimeKind]::Utc)) { throw 'Model date wrong' }
    $checks += 3
    $utc = [TimeZoneInfo]::Utc
    if ($text::LastReport($status, $utc) -ne '14 Nov 10:05 (ok)') { throw "Last report text: $($text::LastReport($status, $utc))" }
    if ($text::Calibrated($status, $utc) -ne '12 Nov 2026') { throw 'Calibrated text wrong' }
    $checks += 2

    # Texts without data.
    $missing = [EcoWattSense.Client.Gui.Viewer.StatusInfo]::Missing
    if ($text::LastReport($missing, $utc) -ne 'Waiting for the first report') { throw 'No-report text wrong' }
    if ($text::Calibrated($missing, $utc) -ne 'Not calibrated') { throw 'Not-calibrated text wrong' }
    $checks += 2

    # Level lines (SPEC 3.4) and the unknown case.
    $lines = @{ L1 = 'Whole computer measured'; L2 = 'Processor and memory measured, the rest from calibration';
                L3 = 'Estimated from calibration'; L4 = 'Not measured; the server estimates' }
    foreach ($level in $lines.Keys) {
        if ($text::LevelLine($level) -ne $lines[$level]) { throw "Level line wrong for $level" }
        if ($text::LevelWord($level) -ne $level) { throw "Level word wrong for $level" }
        $checks += 2
    }
    if ($text::LevelLine($null) -ne 'No level reported yet' -or $text::LevelLine('L7') -ne 'No level reported yet') { throw 'Unknown level line wrong' }
    $checks++

    # Device id, power and energy texts: range checks.
    if ($text::ShortId("0f3a9c1e-1111-2222-3333-444455556666`n") -ne '0f3a9c1e') { throw 'Short id wrong' }
    foreach ($badId in @($null, '', 'abc', 'zzzzzzzz-0000')) {
        if ($text::ShortId($badId) -ne 'Unknown') { throw "Bad id accepted: $badId" }
        $checks++
    }
    if ($text::Watts([Nullable[double]]12.34) -ne '12.3 W' -or $text::Watts([Nullable[double]]$null) -ne 'Unknown' -or
        $text::PowerNote($true) -ne 'Processor and memory only. Calibrate to see the whole computer.' -or $text::PowerNote($false) -ne '' -or
        $text::Kilowatthours([Nullable[double]]0.426, $true) -ne '0.43 kWh (processor and memory only)' -or $text::Kilowatthours([Nullable[double]]0.426, $false) -ne '0.43 kWh' -or
        $text::Watts([Nullable[double]]-1) -ne 'Unknown' -or $text::Watts([Nullable[double]][double]::NaN) -ne 'Unknown') { throw 'Watts text wrong' }
    if ($text::Kilowatthours([Nullable[double]]0.426) -ne '0.43 kWh' -or $text::Kilowatthours([Nullable[double]]$null) -ne 'Unknown' -or
        $text::Kilowatthours([Nullable[double]][double]::PositiveInfinity) -ne 'Unknown') { throw 'kWh text wrong' }
    if ($text::PowerSource([Nullable[bool]]$true) -ne 'Battery' -or $text::PowerSource([Nullable[bool]]$false) -ne 'Mains' -or
        $text::PowerSource([Nullable[bool]]$null) -ne 'Unknown') { throw 'Power source text wrong' }
    $checks += 3
    Write-Output "Passed $checks status-reader checks."
} finally {
    $resolved = [IO.Path]::GetFullPath($directory)
    $parent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar)
    if ([IO.Path]::GetDirectoryName($resolved) -ne $parent -or ![IO.Path]::GetFileName($resolved).StartsWith('EcoWattSense-StatusTest-')) { throw 'Unsafe cleanup path' }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
