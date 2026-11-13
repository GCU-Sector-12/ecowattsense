# Test-Sender.ps1: live test of the WinHTTP sender of ews-core against a local HTTP listener.
# Who uses it: the author of the native code and the main session, after Test-Native.ps1 has built the tests.
# What it does: for each scenario it starts a System.Net.HttpListener on http://localhost:<port>/ (a free port from
#   49200 to 49300; no administrator rights are needed for localhost), runs `ews-core-tests.exe --send <base-url>`,
#   answers the one request, and compares the printed line with the expected one. It also checks that the request had
#   the path /api/reports, the method POST, the header X-EWS-Key: test-key, a JSON content type and a JSON array body
#   of two items. The last scenario has no listener at all (a closed port) and expects `network`.
#   Nothing is sent to a real server. The listener is always stopped, also when a scenario fails.
# How to run: pwsh -NoProfile -File Test-Sender.ps1 [-Exe <path to ews-core-tests.exe>]
# Output: one line per scenario (PASS or FAIL with the reason).
# Exit code: the number of failed scenarios (0 = all passed).
param(
    [string] $Exe = (Join-Path $PSScriptRoot '..\native\out\Release\ews-core-tests.exe')
)

$ErrorActionPreference = 'Stop'

<#
.SYNOPSIS Finds a free TCP port on the loopback address between 49200 and 49300.
.OUTPUTS The port number.
#>
function Get-FreePort {
    foreach ($port in 49200..49300) {
        $probe = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $port)
        try { $probe.Start(); return $port } catch { continue } finally { $probe.Stop() }
    }
    throw 'no free port between 49200 and 49300'
}

<#
.SYNOPSIS Runs the test program in --send mode and returns what it printed.
.PARAMETER BaseUrl The server URL given to the program.
.OUTPUTS A hashtable with Process and OutputFile; the caller waits for the process.
#>
function Start-SendProcess([string] $BaseUrl) {
    $outFile = [System.IO.Path]::GetTempFileName()
    $process = Start-Process -FilePath $Exe -ArgumentList @('--send', $BaseUrl) -NoNewWindow -PassThru -RedirectStandardOutput $outFile
    return @{ Process = $process; OutputFile = $outFile }
}

<#
.SYNOPSIS Waits for the --send process, reads its single output line and removes the temporary file.
.OUTPUTS The printed line (trimmed), or an empty text when the process did not finish.
#>
function Receive-SendOutput($Started) {
    $finished = $Started.Process.WaitForExit(20000)
    $line = $finished ? ((Get-Content -LiteralPath $Started.OutputFile -Raw) ?? '').Trim() : ''
    Remove-Item -LiteralPath $Started.OutputFile -Force -ErrorAction SilentlyContinue
    return $line
}

<#
.SYNOPSIS Checks the one request the listener received.
.OUTPUTS An empty text when the request is right, otherwise the reason.
#>
function Test-Request($Context) {
    $request = $Context.Request
    $reader = [System.IO.StreamReader]::new($request.InputStream, [System.Text.Encoding]::UTF8)
    $body = $reader.ReadToEnd()
    $items = $body | ConvertFrom-Json -NoEnumerate
    $problems = @()
    if ($request.HttpMethod -ne 'POST') { $problems += 'method is not POST' }
    if ($request.Url.AbsolutePath -ne '/api/reports') { $problems += 'path is not /api/reports' }
    if ($request.Headers['X-EWS-Key'] -ne 'test-key') { $problems += 'header X-EWS-Key is wrong' }
    if ($request.ContentType -notlike 'application/json*') { $problems += 'content type is not JSON' }
    if (-not $body.StartsWith('[') -or -not $body.EndsWith(']')) { $problems += 'body is not a JSON array' }
    if (@($items).Count -ne 2) { $problems += 'body does not hold two items' }
    return ($problems -join '; ')
}

<#
.SYNOPSIS Sends the answer of a scenario and closes the response.
#>
function Send-Answer($Context, [int] $Status, [string] $Body) {
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Body)
    $Context.Response.StatusCode = $Status
    $Context.Response.ContentType = 'application/json'
    $Context.Response.ContentLength64 = $bytes.Length
    $Context.Response.OutputStream.Write($bytes, 0, $bytes.Length)
    $Context.Response.Close()
}

<#
.SYNOPSIS Runs one scenario with a listener that answers once.
.OUTPUTS The failure reason, or an empty text when the scenario passed.
#>
function Invoke-Answered([int] $Status, [string] $Body, [string] $Expected) {
    $port = Get-FreePort
    $listener = [System.Net.HttpListener]::new()
    $listener.Prefixes.Add("http://localhost:$port/")
    $started = $null
    try {
        $listener.Start()
        $started = Start-SendProcess "http://localhost:$port"
        $pending = $listener.GetContextAsync()
        if (-not $pending.Wait(15000)) { return 'the program sent no request' }
        $context = $pending.Result
        $requestProblem = Test-Request $context
        Send-Answer $context $Status $Body
        $printed = Receive-SendOutput $started
        if ($requestProblem) { return "request: $requestProblem" }
        if ($printed -ne $Expected) { return "printed '$printed', expected '$Expected'" }
        return ''
    }
    finally {
        $listener.Stop()
        $listener.Close()
        if ($started -and -not $started.Process.HasExited) { $started.Process.Kill() }
    }
}

<#
.SYNOPSIS Runs the scenario with no listener (a closed port): the sender must report `network`.
.OUTPUTS The failure reason, or an empty text when the scenario passed.
#>
function Invoke-ClosedPort([string] $Expected) {
    $port = Get-FreePort
    $started = Start-SendProcess "http://localhost:$port"
    $printed = Receive-SendOutput $started
    if ($printed -ne $Expected) { return "printed '$printed', expected '$Expected'" }
    return ''
}

if (-not (Test-Path -LiteralPath $Exe)) {
    Write-Host "FAIL: test program not found: $Exe (run Test-Native.ps1 first)"
    exit 1
}

$answerWithOneRejected = '[{"status":"created"},{"status":"rejected","message":"x"}]'
$scenarios = @(
    @{ Name = '200 with one rejected item'; Run = { Invoke-Answered 200 $answerWithOneRejected 'ok leave=1 rejected=1' } },
    @{ Name = '401 unauthorised';           Run = { Invoke-Answered 401 '{"detail":"no"}' 'unauthorised leave=0 rejected=0' } },
    @{ Name = '500 server error';           Run = { Invoke-Answered 500 '' 'server_error leave=0 rejected=0' } },
    @{ Name = '422 rejected';               Run = { Invoke-Answered 422 '{"detail":[]}' 'rejected leave=1 rejected=2' } },
    @{ Name = '429 too many requests';      Run = { Invoke-Answered 429 '' 'server_error leave=0 rejected=0' } },
    @{ Name = 'closed port (network)';      Run = { Invoke-ClosedPort 'network leave=0 rejected=0' } }
)

$failed = 0
foreach ($scenario in $scenarios) {
    $reason = & $scenario.Run
    $verdict = $reason ? "FAIL ($reason)" : 'PASS'
    $failed += $reason ? 1 : 0
    Write-Host ("{0,-30} {1}" -f $scenario.Name, $verdict)
}
Write-Host "failed scenarios: $failed"
exit $failed
