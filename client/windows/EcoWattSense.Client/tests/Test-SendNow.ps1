# Run with PowerShell 7 (.NET 10): pwsh -File ./tests/Test-SendNow.ps1
# Checks the waiting-reports line, the Send now button state, the exact pipe text, the 30 s lock and the status.json fields
# "queued" and "server_set" (server_url plus server_key_set).
$ErrorActionPreference = 'Stop'
$viewer = Join-Path $PSScriptRoot '../../EcoWattSense.Client.Gui/Viewer'
Add-Type -Path @((Join-Path $viewer 'ServicePipe.cs'), (Join-Path $viewer 'StatusReader.cs'), (Join-Path $viewer 'SendNowState.cs'))
$state = [EcoWattSense.Client.Gui.Viewer.SendNowState]
$reader = [EcoWattSense.Client.Gui.Viewer.StatusReader]
$checks = 0
function Check($ok, $what) { if (!$ok) { throw "Failed: $what" }; $script:checks++ }
$none = [Nullable[int]]$null
$now = [datetime]::UtcNow
$free = [datetime]::MinValue

# The line text.
Check ($state::QueueLine($none) -eq '') 'missing field: no line'
Check ($state::QueueLine([Nullable[int]]0) -eq 'All reports sent.') '0: all sent'
Check ($state::QueueLine([Nullable[int]]3) -eq 'Waiting to send: 3 reports') '3 reports'
Check ($state::Note([Nullable[int]]3, $true, $false) -eq 'Sending...') 'sending line'
Check ($state::Note([Nullable[int]]3, $true, $true) -eq 'The service did not answer.') 'failure line wins'
Check ($state::Note([Nullable[int]]3, $false, $false) -eq 'Waiting to send: 3 reports') 'normal line after the change'

# The button: queued > 0, server set, lock over.
Check ($state::ButtonEnabled([Nullable[int]]2, $true, $now, $free)) 'queued, server set: enabled'
Check (-not $state::ButtonEnabled([Nullable[int]]0, $true, $now, $free)) 'nothing queued: disabled'
Check (-not $state::ButtonEnabled($none, $true, $now, $free)) 'queued missing: disabled'
Check (-not $state::ButtonEnabled([Nullable[int]]2, $false, $now, $free)) 'no server: disabled'
Check (-not $state::ButtonEnabled([Nullable[int]]0, $false, $now, $free)) 'nothing queued and no server: disabled'

# The 30 s lock.
Check ($state::LockSeconds -eq 30) 'lock is 30 seconds'
$until = $now.AddSeconds(30)
Check (-not $state::ButtonEnabled([Nullable[int]]2, $true, $now.AddSeconds(29), $until)) 'locked at 29 s'
Check ($state::ButtonEnabled([Nullable[int]]2, $true, $now.AddSeconds(30), $until)) 'free at 30 s'

# The exact pipe text, and a real write to a test pipe.
Check ($state::Message -ceq "EWS1 send`n") 'message text'
$name = 'EcoWattSense.Test.' + [guid]::NewGuid().ToString('N')
$server = [IO.Pipes.NamedPipeServerStream]::new($name, 'In', 1, 'Message', 'Asynchronous', 256, 256)
try {
    $sender = [EcoWattSense.Client.Gui.Viewer.ServicePipeSender]::new($name, 2000)
    $accept = $server.WaitForConnectionAsync()
    Check ($sender.Send($state::Message)) 'send to a live pipe'
    $accept.Wait(2000) | Out-Null
    $buffer = New-Object byte[] 64
    $count = $server.Read($buffer, 0, 64)
    Check ([Text.Encoding]::ASCII.GetString($buffer, 0, $count) -ceq "EWS1 send`n") 'pipe received exactly EWS1 send'
} finally { $server.Dispose() }
$dead = [EcoWattSense.Client.Gui.Viewer.ServicePipeSender]::new('EcoWattSense.Test.none.' + [guid]::NewGuid().ToString('N'), 100)
Check (-not $dead.Send($state::Message)) 'missing pipe: send fails'

# status.json fields.
$good = $reader::Parse('{"queued":4,"server_url":"https://x.example","server_key_set":true}')
Check (($good.Queued -eq 4) -and $good.ServerSet) 'queued 4, server set'
$noKey = $reader::Parse('{"queued":0,"server_url":"https://x.example","server_key_set":false}')
Check (($noKey.Queued -eq 0) -and -not $noKey.ServerSet) 'key not set'
$noUrl = $reader::Parse('{"queued":1,"server_url":"","server_key_set":true}')
Check (-not $noUrl.ServerSet) 'empty url'
$bare = $reader::Parse('{}')
Check (($null -eq $bare.Queued) -and -not $bare.ServerSet) 'missing fields'
foreach ($bad in '"queued":-1', '"queued":"2"', '"queued":1.5', '"queued":99999999999') {
    Check ($null -eq $reader::Parse("{$bad}").Queued) "bad queued $bad"
}
Write-Output "Passed $checks send now checks."
