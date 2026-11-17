# Run with PowerShell 7 (.NET 10): pwsh -File ./tests/Test-HistoryReader.ps1
# Compiles the real HistoryRow and HistoryReader (no WPF) and reads history files made in a temp folder.
$ErrorActionPreference = 'Stop'
$gui = Join-Path $PSScriptRoot '../../EcoWattSense.Client.Gui/History'
Add-Type -Path @((Join-Path $gui 'HistoryRow.cs'), (Join-Path $gui 'HistoryReader.cs'))
$header = [EcoWattSense.Client.Gui.History.HistoryReader]::Header
$now = [datetime]::new(2026, 11, 14, 10, 0, 0, [DateTimeKind]::Utc)
$folder = Join-Path ([IO.Path]::GetTempPath()) ('EcoWattSense-ReaderTest-' + [guid]::NewGuid().ToString('N'))
$checks = 0
function Check($ok, $what) { if (!$ok) { throw "Failed: $what" }; $script:checks++ }
function Row($ts, $bat = 0, $state = 2, $dev = '4.5') { "v1,$ts,60,$bat,$state,10,5.0,$dev,2.0,0.000,L2" }
function Put($name, $lines) { [IO.File]::WriteAllText((Join-Path $folder $name), (($lines -join "`n") + "`n")) }
function Read() { [EcoWattSense.Client.Gui.History.HistoryReader]::Read($folder, $now) }
try {
    # Folder missing: no rows, no error.
    $r = Read
    Check ($r.Rows.Count -eq 0 -and $r.BadRows -eq 0 -and $r.FilesRead -eq 0 -and $r.FilesUnreadable -eq 0) 'missing folder'
    New-Item -ItemType Directory $folder | Out-Null

    # Empty file and header only: read, no rows.
    [IO.File]::WriteAllText((Join-Path $folder '2026-11-14.csv'), '')
    $r = Read
    Check ($r.FilesRead -eq 1 -and $r.Rows.Count -eq 0 -and $r.FilesUnreadable -eq 0) 'empty file'
    Put '2026-11-14.csv' @($header)
    $r = Read
    Check ($r.FilesRead -eq 1 -and $r.Rows.Count -eq 0) 'header only'

    # Wrong header: file unreadable, rows not used.
    Put '2026-11-14.csv' @('v2,ts_utc', (Row '2026-11-14T09:00:00Z'))
    $r = Read
    Check ($r.FilesUnreadable -eq 1 -and $r.FilesRead -eq 0 -and $r.Rows.Count -eq 0) 'wrong header'

    # Bad rows counted: wrong field count, bad time, out of range, bad state, bad level, battery flag 2.
    Put '2026-11-14.csv' @($header,
        (Row '2026-11-14T09:00:00Z'),
        'v1,2026-11-14T09:01:00Z,60,0',
        (Row 'yesterday'),
        (Row '2026-11-14T09:02:00Z' 0 2 '99999'),
        (Row '2026-11-14T09:03:00Z' 0 7),
        (Row '2026-11-14T09:04:00Z' 2),
        (Row '2026-11-14T09:05:00Z' 0 2 'NaN'),
        'v1,2026-11-14T09:06:00Z,60,0,2,10,5.0,4.5,2.0,0.000,LX',
        '',
        (Row '2026-11-14T09:07:00Z'))
    $r = Read
    Check ($r.Rows.Count -eq 2 -and $r.BadRows -eq 7) "bad rows counted: rows=$($r.Rows.Count) bad=$($r.BadRows)"

    # Empty fields are null; a gap (missing minutes) just gives no rows.
    Put '2026-11-14.csv' @($header,
        'v1,2026-11-14T09:00:00Z,60,1,,,,,,,',
        (Row '2026-11-14T09:05:00Z'))
    $r = Read
    $a = $r.Rows[0]
    Check ($r.Rows.Count -eq 2 -and $r.BadRows -eq 0 -and $a.OnBattery -and $null -eq $a.State -and $null -eq $a.DeviceW -and $null -eq $a.Level) 'empty fields and gap'
    Check ($r.Rows[1].DeviceW -eq 4.5 -and $r.Rows[1].Level -eq 'L2' -and $r.Rows[1].State -eq 2) 'values parsed'

    # A last line without a line break may be half written: it is dropped, not counted.
    [IO.File]::WriteAllText((Join-Path $folder '2026-11-14.csv'), ($header + "`n" + (Row '2026-11-14T09:00:00Z') + "`n" + 'v1,2026-11-14T09:01:00Z,60,0,2,10,5.0,4'))
    $r = Read
    Check ($r.Rows.Count -eq 1 -and $r.BadRows -eq 0) 'half written last line'

    # Two files across midnight UTC; rows older than 24 h left out; order is oldest first.
    Put '2026-11-13.csv' @($header,
        (Row '2026-11-13T09:59:00Z'),
        (Row '2026-11-13T10:01:00Z'),
        (Row '2026-11-13T23:59:00Z'))
    Put '2026-11-14.csv' @($header, (Row '2026-11-14T00:01:00Z'), (Row '2026-11-14T09:59:00Z'))
    $r = Read
    Check ($r.FilesRead -eq 2 -and $r.Rows.Count -eq 4) "two files: rows=$($r.Rows.Count)"
    Check ($r.Rows[0].EndUtc -eq [datetime]::new(2026, 11, 13, 10, 1, 0, [DateTimeKind]::Utc) -and $r.Rows[3].EndUtc.Hour -eq 9) 'order across midnight'

    # File opened for append by the writer: shared read must work.
    $writer = [IO.File]::Open((Join-Path $folder '2026-11-14.csv'), 'Open', 'Write', 'Read')
    try { $r = Read } finally { $writer.Dispose() }
    Check ($r.FilesRead -eq 2 -and $r.FilesUnreadable -eq 0) 'file open by the writer'

    # All rows bad: no rows, all counted.
    Put '2026-11-14.csv' @($header, 'junk', 'more,junk')
    Remove-Item (Join-Path $folder '2026-11-13.csv')
    $r = Read
    Check ($r.Rows.Count -eq 0 -and $r.BadRows -eq 2) 'all rows bad'
    Write-Output "Passed $checks history reader checks."
} finally {
    $resolved = [IO.Path]::GetFullPath($folder)
    $parent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar)
    if ([IO.Path]::GetDirectoryName($resolved) -ne $parent -or ![IO.Path]::GetFileName($resolved).StartsWith('EcoWattSense-ReaderTest-')) { throw 'Unsafe cleanup' }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
