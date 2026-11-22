# Test-ServiceConsole.ps1: live test of ews-service.exe in console mode on this PC, as a standard user, with no real server.
# Who uses it: the author of the native code and the main session, after Test-Native.ps1 has built the Release programs.
# What it does: makes a data folder below native\out with the sub folders of SPEC 2.2 and the example model of SPEC 8,
#   writes service.ini (interval 300 s), sets the URL and the key through `ews-service.exe --configure` on standard input,
#   starts a local HttpListener on a free port from 49301 to 49400 that answers 200 and keeps every request, runs
#   `ews-service.exe --console --tick-seconds 2 --run-seconds 34`, sends one pipe message `EWS1 idle=5 bright=50` after about
#   12 seconds, and checks the files and the reports that arrived. Nothing is sent to a real server.
# How to run: pwsh -NoProfile -File Test-ServiceConsole.ps1 [-Exe <path to ews-service.exe>] [-Keep]
#   -Keep leaves the data folder in place (it is below native\out, which is deleted at the end of the work anyway).
# Output: one PASS or FAIL line per check.
# Exit code: the number of failed checks (0 = all passed). The listener is always stopped.
param(
    [string] $Exe = (Join-Path $PSScriptRoot '..\native\out\Release\ews-service.exe'),
    [switch] $Keep
)

$ErrorActionPreference = 'Stop'
$script:failed = 0
$script:received = [System.Collections.Generic.List[object]]::new()

$requiredFields = 'device_id', 'timestamp', 'interval_seconds', 'idle_seconds', 'state', 'cpu_percent', 'screen_locked', 'on_battery', 'os', 'device_type'
$energyFields = 'energy_uj', 'battery_energy_uj', 'device_energy_uj'
$specModel = @'
{
  "schema": 2,
  "generated_utc": "2026-11-12T17:08:31Z",
  "machine": { "maker": "HP", "model": "HP 250 G7 Notebook PC", "cpu": "x", "logical_processors": 8, "windows": "10.0.26300.0" },
  "measured_parts": ["cpu_package", "dram"],
  "model": {
    "base_w": 3.48,
    "rest_w": 2.24,
    "rest_w_per_cpu_pct": 0.009,
    "cpu_w_per_pct": 0.075,
    "screen_w_at_50": 1.40,
    "screen_w_per_pct": 0.0104,
    "graphics_w": 2.44
  },
  "phase_power_w": { },
  "cpu_share_by_phase": { },
  "warnings": []
}
'@

<#
.SYNOPSIS Prints one PASS or FAIL line and counts the failures.
.PARAMETER Name The check.
.PARAMETER Passed The result.
.PARAMETER Detail Text shown after a FAIL.
#>
function Write-Check([string] $Name, [bool] $Passed, [string] $Detail = '') {
    $verdict = $Passed ? 'PASS' : "FAIL $Detail".TrimEnd()
    $script:failed += $Passed ? 0 : 1
    Write-Host ("{0,-62} {1}" -f $Name, $verdict)
}

<#
.SYNOPSIS Finds a free TCP port on the loopback address between 49301 and 49400.
.OUTPUTS The port number.
#>
function Get-FreePort {
    foreach ($port in 49301..49400) {
        $probe = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $port)
        try { $probe.Start(); return $port } catch { continue } finally { $probe.Stop() }
    }
    throw 'no free port between 49301 and 49400'
}

<#
.SYNOPSIS Makes the data folder with the sub folders of SPEC 2.2 and the model file.
.PARAMETER Folder The data folder.
#>
function New-DataFolder([string] $Folder) {
    foreach ($name in 'Client', 'scan', 'calibration', 'history', 'status', 'queue', 'config', 'logs') {
        New-Item -ItemType Directory -Force -Path (Join-Path $Folder $name) | Out-Null
    }
    Set-Content -LiteralPath (Join-Path $Folder 'calibration\model.json') -Value $specModel -Encoding utf8NoBOM
    Set-Content -LiteralPath (Join-Path $Folder 'config\service.ini') -Value "interval_seconds=300`n" -Encoding ascii -NoNewline
}

<#
.SYNOPSIS Runs `ews-service.exe --configure` and gives the key and the URL on standard input.
.PARAMETER Folder The data folder.
.PARAMETER Url The server URL.
.OUTPUTS A hashtable with the exit code and the printed word.
#>
function Invoke-Configure([string] $Folder, [string] $Url) {
    $info = [System.Diagnostics.ProcessStartInfo]::new($Exe)
    $info.ArgumentList.Add('--configure')
    $info.ArgumentList.Add('--data-dir')
    $info.ArgumentList.Add($Folder)
    $info.RedirectStandardInput = $true
    $info.RedirectStandardOutput = $true
    $info.UseShellExecute = $false
    $process = [System.Diagnostics.Process]::Start($info)
    $process.StandardInput.Write("url=$Url`nkey=test-key`n")
    $process.StandardInput.Close()
    $word = $process.StandardOutput.ReadToEnd().Trim()
    $process.WaitForExit()
    return @{ Code = $process.ExitCode; Word = $word }
}

<#
.SYNOPSIS Reads one request of the service, keeps it, and answers 200.
.PARAMETER Context The HttpListener context.
.PARAMETER Seconds Seconds since the program started.
#>
function Receive-Request($Context, [double] $Seconds) {
    $reader = [System.IO.StreamReader]::new($Context.Request.InputStream, [System.Text.Encoding]::UTF8)
    $body = $reader.ReadToEnd()
    $script:received.Add([pscustomobject]@{ Body = $body; Key = $Context.Request.Headers['X-EWS-Key']; At = $Seconds })
    $answer = [System.Text.Encoding]::UTF8.GetBytes('[{"status":"created"}]')
    $Context.Response.StatusCode = 200
    $Context.Response.ContentType = 'application/json'
    $Context.Response.ContentLength64 = $answer.Length
    $Context.Response.OutputStream.Write($answer, 0, $answer.Length)
    $Context.Response.Close()
}

<#
.SYNOPSIS Sends the pipe message of a user session to the service.
.OUTPUTS An empty text when it was sent, otherwise the error.
#>
function Send-PipeMessage {
    $client = [System.IO.Pipes.NamedPipeClientStream]::new('.', 'EcoWattSense.Service.Session', [System.IO.Pipes.PipeDirection]::Out)
    try {
        $client.Connect(3000)
        $bytes = [System.Text.Encoding]::ASCII.GetBytes("EWS1 idle=5 bright=50`n")
        $client.Write($bytes, 0, $bytes.Length)
        $client.Flush()
        return ''
    }
    catch { return $_.Exception.Message }
    finally { $client.Dispose() }
}

<#
.SYNOPSIS Runs the service for the test and serves the listener until the program ends.
.PARAMETER Folder The data folder.
.PARAMETER Listener The started HttpListener.
.OUTPUTS A hashtable with the exit code, the second the pipe message was sent (or -1) and the pipe error text.
#>
function Invoke-ServiceRun([string] $Folder, $Listener) {
    $process = Start-Process -FilePath $Exe -ArgumentList @('--console', '--data-dir', "`"$Folder`"", '--tick-seconds', '2', '--run-seconds', '34') -NoNewWindow -PassThru
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    $pipeAt = -1.0
    $pipeError = 'not sent'
    $pending = $null
    while (-not $process.HasExited) {
        $pending = $pending ?? $Listener.GetContextAsync()
        $arrived = $pending.Wait(300)
        if ($arrived) { Receive-Request $pending.Result $watch.Elapsed.TotalSeconds; $pending = $null }
        $due = $pipeAt -lt 0 -and $watch.Elapsed.TotalSeconds -ge 12
        if ($due) { $pipeAt = $watch.Elapsed.TotalSeconds; $pipeError = Send-PipeMessage }
    }
    $process.WaitForExit()
    return @{ Code = $process.ExitCode; PipeAt = $pipeAt; PipeError = $pipeError }
}

<#
.SYNOPSIS Reads a JSON file, or returns null when it is missing or not JSON.
#>
function Read-JsonFile([string] $Path) {
    try { return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json } catch { return $null }
}

<#
.SYNOPSIS Collects every report of every request, each with the second its request arrived.
#>
function Get-Reports {
    $reports = [System.Collections.Generic.List[object]]::new()
    foreach ($request in $script:received) {
        $items = $request.Body | ConvertFrom-Json -NoEnumerate
        foreach ($item in $items) { $reports.Add([pscustomobject]@{ Report = $item; At = $request.At }) }
    }
    return $reports
}

<#
.SYNOPSIS Checks the fields of one report against SPEC 5.4. @OUTPUTS An empty text, or what is wrong.
#>
function Test-ReportShape($Report) {
    $names = $Report.PSObject.Properties.Name
    $missing = $requiredFields | Where-Object { $names -notcontains $_ }
    $unknown = $names | Where-Object { ($requiredFields + $energyFields) -notcontains $_ }
    $problems = @()
    if ($missing) { $problems += "missing: $($missing -join ',')" }
    if ($unknown) { $problems += "unknown: $($unknown -join ',')" }
    if ($Report.os -ne 'windows') { $problems += 'os is not windows' }
    $parsed = [guid]::Empty
    $isGuid = [guid]::TryParse([string]$Report.device_id, [ref]$parsed) -and ([string]$Report.device_id -ceq $parsed.ToString('D'))
    if (-not $isGuid) { $problems += 'device_id is not a lower case GUID' }
    return ($problems -join '; ')
}

<#
.SYNOPSIS Checks the energy fields of one 10 second report: on mains 30 to 600 J and a battery change, on battery zero.
#>
function Test-ReportEnergy($Report) {
    $names = $Report.PSObject.Properties.Name
    if ($Report.on_battery) { return ($names -contains 'energy_uj' -and $Report.energy_uj -eq 0) }
    $hasAll = ($energyFields | Where-Object { $names -notcontains $_ }).Count -eq 0
    return $hasAll -and $Report.energy_uj -ge 30000000 -and $Report.energy_uj -le 600000000
}

<#
.SYNOPSIS Runs every check on the finished run.
#>
function Test-Results([string] $Folder, $Run) {
    Write-Check 'exit code is 0' ($Run.Code -eq 0) "(exit $($Run.Code))"
    $caps = Read-JsonFile (Join-Path $Folder 'scan\capabilities.service.json')
    Write-Check 'capabilities.service.json parses, level L2' ($null -ne $caps -and $caps.level -eq 'L2') "(level $($caps.level))"
    $history = Get-ChildItem -LiteralPath (Join-Path $Folder 'history') -Filter '*.csv' | ForEach-Object { Get-Content -LiteralPath $_.FullName } | Select-Object -Skip 1
    $rows = @($history | Where-Object { $_ }).Count
    Write-Check 'at least 14 history rows after the header' ($rows -ge 14) "($rows rows)"
    $idleValues = @($history | Where-Object { $_ } | ForEach-Object { [int]($_ -split ',')[5] })
    $dropped = 0..([Math]::Max($idleValues.Count - 2, 0)) | Where-Object { $idleValues.Count -gt 1 -and $idleValues[$_ + 1] -lt $idleValues[$_] }
    Write-Check 'pipe message was taken: idle_s in the history dropped' ([bool]$dropped -and $Run.PipeError -eq '') "(pipe: $($Run.PipeError))"
    $reports = Get-Reports
    Write-Check 'at least 2 reports received' ($reports.Count -ge 2) "($($reports.Count) reports)"
    $shapeProblems = $reports | ForEach-Object { Test-ReportShape $_.Report } | Where-Object { $_ }
    Write-Check 'every report has exactly the fields of SPEC 5.4' (-not $shapeProblems) "($($shapeProblems -join ' | '))"
    $badLength = $reports | Where-Object { $_.Report.interval_seconds -lt 8 -or $_.Report.interval_seconds -gt 12 }
    Write-Check 'interval_seconds is 8 to 12 in every sent report' (-not $badLength) "($(@($badLength).Count) outside)"
    $badEnergy = $reports | Where-Object { -not (Test-ReportEnergy $_.Report) }
    Write-Check 'energy fields right (mains: 30 to 600 J and battery change)' (-not $badEnergy) "($(@($badEnergy).Count) wrong)"
    $afterPipe = $reports | Where-Object { $_.At -gt $Run.PipeAt -and $Run.PipeAt -ge 0 }
    $badPipe = $afterPipe | Where-Object { $_.Report.state -ne 2 -or $_.Report.idle_seconds -ge 60 }
    Write-Check 'reports after the pipe message: state 2, idle below 60' (@($afterPipe).Count -ge 1 -and -not $badPipe) "(pipe: $($Run.PipeError); $(@($afterPipe).Count) reports after it)"
    $badKey = $script:received | Where-Object { $_.Key -ne 'test-key' }
    Write-Check 'header X-EWS-Key was test-key' ($script:received.Count -ge 1 -and -not $badKey)
    $status = Read-JsonFile (Join-Path $Folder 'status\status.json')
    Write-Check 'status.json parses and says ok' ($null -ne $status -and $status.last_send_result -eq 'ok') "(result $($status.last_send_result))"
    $queue = Get-Content -LiteralPath (Join-Path $Folder 'queue\reports.jsonl') -ErrorAction SilentlyContinue
    $queued = @($queue | Where-Object { $_ }).Count
    Write-Check 'queue is empty or holds only the report of the stop' ($queued -le 1) "($queued lines)"
    $log = Get-Content -LiteralPath (Join-Path $Folder 'logs\service.log') -Raw -ErrorAction SilentlyContinue
    Write-Check 'service.log has neither the key nor localhost' ($null -ne $log -and $log -notmatch 'test-key' -and $log -notmatch 'localhost')
    Write-Host ('first request body: ' + ($script:received | Select-Object -First 1).Body)
}

if (-not (Test-Path -LiteralPath $Exe)) {
    Write-Host "FAIL: program not found: $Exe (run Test-Native.ps1 first)"
    exit 1
}

$native = (Resolve-Path (Join-Path $PSScriptRoot '..\native')).Path
$folder = Join-Path $native "out\svc-console-test-$PID"
$port = Get-FreePort
$listener = [System.Net.HttpListener]::new()
$listener.Prefixes.Add("http://localhost:$port/")
try {
    New-DataFolder $folder
    $configure = Invoke-Configure $folder "http://localhost:$port"
    Write-Check '--configure prints ok and exits 0' ($configure.Code -eq 0 -and $configure.Word -eq 'ok') "(exit $($configure.Code), word '$($configure.Word)')"
    $listener.Start()
    $run = Invoke-ServiceRun $folder $listener
    Test-Results $folder $run
}
finally {
    $listener.Stop()
    $listener.Close()
    if (-not $Keep) { Remove-Item -LiteralPath $folder -Recurse -Force -ErrorAction SilentlyContinue }
}
Write-Host "failed checks: $script:failed"
exit $script:failed
