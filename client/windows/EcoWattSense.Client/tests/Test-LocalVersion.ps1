# Run with PowerShell 7 (.NET 10): pwsh -File ./tests/Test-LocalVersion.ps1
# Compiles the real LocalVersionRules and LocalVersionServer (no WPF) and checks the request parser, the reply builder
# (GET, OPTIONS with and without the private-network header, unknown path, unknown origin, oversized head, bad request line)
# and one live round trip on a free loopback port.
$ErrorActionPreference = 'Stop'
$folder = Join-Path $PSScriptRoot '../../EcoWattSense.Client.Gui/LocalVersion'
Add-Type -Path (Join-Path $folder 'LocalVersionRules.cs'), (Join-Path $folder 'LocalVersionServer.cs')
$checks = 0
function Check($ok, $what) { if (!$ok) { throw "Failed: $what" }; $script:checks++ }
$rules = [EcoWattSense.Client.Gui.LocalVersion.LocalVersionRules]
$allowed = 'https://dev.ecowattsense.co.uk'

function Reply($head) { return $rules::BuildResponse($rules::ParseHead($head), '1.2.3') }
function Request($method, $path, $extra) {
    return "$method $path HTTP/1.1`r`nHost: 127.0.0.1:47613`r`n$extra"
}

# GET /version
$get = Reply (Request 'GET' '/version' '')
Check ($get.StartsWith("HTTP/1.1 200 OK`r`n")) 'GET /version is 200'
Check ($get.EndsWith("`r`n`r`n{`"version`":`"1.2.3`"}")) 'the body is the version JSON'
Check ($get.Contains('Content-Type: application/json')) 'the content type is JSON'
Check ($get.Contains('Content-Length: 19')) 'the length matches the body'
Check (!$get.Contains('Access-Control-')) 'no origin, no CORS headers'

# CORS for the three allowed origins
foreach ($origin in $allowed, 'http://localhost:5173', 'http://127.0.0.1:5173') {
    $reply = Reply (Request 'GET' '/version' "Origin: $origin`r`n")
    Check ($reply.Contains("Access-Control-Allow-Origin: $origin`r`n")) "allowed origin $origin"
    Check ($reply.Contains("Vary: Origin`r`n")) 'Vary: Origin'
    Check ($reply.Contains('Access-Control-Allow-Methods: GET')) 'allowed methods'
    Check ($reply.Contains('Access-Control-Max-Age: 600')) 'max age'
}

# Unknown origins get no CORS headers (exact match only)
foreach ($origin in 'https://evil.example', 'https://dev.ecowattsense.co.uk.evil.example', 'http://localhost:5174', 'null') {
    $reply = Reply (Request 'GET' '/version' "Origin: $origin`r`n")
    Check ($reply.StartsWith('HTTP/1.1 200') -and !$reply.Contains('Access-Control-')) "unknown origin $origin has no CORS headers"
}

# OPTIONS preflight
$plain = Reply (Request 'OPTIONS' '/version' "Origin: $allowed`r`nAccess-Control-Request-Method: GET`r`n")
Check ($plain.StartsWith("HTTP/1.1 204 No Content`r`n")) 'OPTIONS is 204'
Check ($plain.Contains("Access-Control-Allow-Origin: $allowed")) 'preflight allows the origin'
Check (!$plain.Contains('Private-Network')) 'no private-network header without the request header'
Check (!$plain.Contains('Content-Length') -and $plain.EndsWith("`r`n`r`n")) '204 has no body'
$private = Reply (Request 'OPTIONS' '/version' "Origin: $allowed`r`naccess-control-request-private-network: true`r`n")
Check ($private.Contains('Access-Control-Allow-Private-Network: true')) 'private-network header answered'
$noOrigin = Reply (Request 'OPTIONS' '/version' "Access-Control-Request-Private-Network: true`r`n")
Check ($noOrigin.StartsWith('HTTP/1.1 204') -and !$noOrigin.Contains('Access-Control-')) 'preflight from an unknown origin gets no CORS headers'

# Everything else is an empty 404
foreach ($head in (Request 'GET' '/' ''), (Request 'GET' '/version/' ''), (Request 'GET' '/version?x=1' ''), (Request 'POST' '/version' ''), (Request 'DELETE' '/version' ''), (Request 'GET' '/status' "Origin: $allowed`r`n")) {
    $reply = Reply $head
    Check ($reply.StartsWith("HTTP/1.1 404 Not Found`r`n") -and $reply.EndsWith("`r`n`r`n") -and $reply.Contains('Content-Length: 0')) "empty 404 for $($head.Split(' ')[0..1] -join ' ')"
    Check (!$reply.Contains('Access-Control-') -and !$reply.Contains('1.2.3')) 'the 404 leaks nothing'
}

# Bad request lines and oversized heads
foreach ($bad in 'GARBAGE', 'GET /version', 'GET /version HTTP/2 extra', 'GET version HTTP/1.1', ' /version HTTP/1.1', 'GET /version FTP/1.0', '') {
    Check ($null -eq $rules::ParseHead($bad)) "bad request line refused: '$bad'"
    Check ((Reply $bad).StartsWith('HTTP/1.1 404')) 'a bad head gets 404'
}
Check ($null -eq $rules::ParseHead($null)) 'a null head is refused'
Check ($null -eq $rules::ParseHead("GET /version HTTP/1.1`r`nX: " + ('a' * 9000))) 'an oversized head is refused'
Check ($rules::HeadComplete([Text.Encoding]::ASCII.GetBytes("GET / HTTP/1.1`r`n`r`n"))) 'head end found'
Check (!$rules::HeadComplete([Text.Encoding]::ASCII.GetBytes("GET / HTTP/1.1`r`n"))) 'head end missing'

# Live: start on a free port, ask, stop
$server = [EcoWattSense.Client.Gui.LocalVersion.LocalVersionServer]::TryStart(0, '9.8.7')
Check ($null -ne $server) 'the server starts on a free port'
try {
    $client = [Net.Http.HttpClient]::new()
    $url = "http://127.0.0.1:$($server.Port)/version"
    $message = [Net.Http.HttpRequestMessage]::new([Net.Http.HttpMethod]::Get, $url)
    $message.Headers.Add('Origin', 'http://localhost:5173')
    $response = $client.SendAsync($message).GetAwaiter().GetResult()
    $text = $response.Content.ReadAsStringAsync().GetAwaiter().GetResult()
    Check ([int]$response.StatusCode -eq 200) 'live GET is 200'
    Check (($text | ConvertFrom-Json).version -eq '9.8.7') 'live GET gives the version'
    Check ($response.Headers.GetValues('Access-Control-Allow-Origin') -eq 'http://localhost:5173') 'live CORS header'
    $missing = $client.GetAsync("http://127.0.0.1:$($server.Port)/other").GetAwaiter().GetResult()
    Check ([int]$missing.StatusCode -eq 404) 'live unknown path is 404'
    # A second server on the same port is skipped and counted.
    $before = [EcoWattSense.Client.Gui.LocalVersion.LocalVersionServer]::SkippedStarts
    Check ($null -eq [EcoWattSense.Client.Gui.LocalVersion.LocalVersionServer]::TryStart($server.Port, '1.0')) 'a busy port is skipped'
    Check ([EcoWattSense.Client.Gui.LocalVersion.LocalVersionServer]::SkippedStarts -eq $before + 1) 'the skip is counted'
    # A client that sends nothing must not block the next one.
    $idle = [Net.Sockets.TcpClient]::new('127.0.0.1', $server.Port)
    $again = $client.GetAsync($url).GetAwaiter().GetResult()
    Check ([int]$again.StatusCode -eq 200) 'a silent connection does not block the next request (after the 2 s limit)'
    $idle.Dispose()
} finally {
    $port = $server.Port
    $server.Dispose()
}
$stopped = $false
try { [Net.Sockets.TcpClient]::new('127.0.0.1', $port).Dispose() } catch { $stopped = $true }
Check $stopped 'the port is closed after Dispose'
Write-Output "Passed $checks local version checks (parser, replies, CORS, 404, oversized head, live round trip)."
