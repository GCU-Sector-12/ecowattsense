# Run with PowerShell 7 (.NET 10): pwsh -File ./tests/Test-ServicePipe.ps1
# Checks the pipe message text (SPEC 6) and the sender against a real test pipe with its own name,
# plus the failure path: a missing pipe is counted and ignored.
$ErrorActionPreference = 'Stop'
Add-Type -Path (Join-Path $PSScriptRoot '../../EcoWattSense.Client.Gui/Viewer/ServicePipe.cs')
$message = [EcoWattSense.Client.Gui.Viewer.ServicePipeMessage]
$checks = 0

# Message text: idle is whole seconds, brightness missing or out of range gives "-".
foreach ($case in @(
    @{ Idle = 0.0; Bright = 50; Expected = "EWS1 idle=0 bright=50`n" },
    @{ Idle = 12.9; Bright = 0; Expected = "EWS1 idle=12 bright=0`n" },
    @{ Idle = 3600.0; Bright = 100; Expected = "EWS1 idle=3600 bright=100`n" },
    @{ Idle = 5.0; Bright = $null; Expected = "EWS1 idle=5 bright=-`n" },
    @{ Idle = 5.0; Bright = 101; Expected = "EWS1 idle=5 bright=-`n" },
    @{ Idle = 5.0; Bright = -1; Expected = "EWS1 idle=5 bright=-`n" },
    @{ Idle = 1e12; Bright = 10; Expected = "EWS1 idle=4294967 bright=10`n" },
    @{ Idle = -1.0; Bright = 10; Expected = $null },
    @{ Idle = [double]::NaN; Bright = 10; Expected = $null },
    @{ Idle = [double]::PositiveInfinity; Bright = 10; Expected = $null }
)) {
    $bright = if ($null -eq $case.Bright) { [Nullable[int]]$null } else { [Nullable[int]]$case.Bright }
    $actual = $message::Format($case.Idle, $bright)
    if ($actual -ne $case.Expected) { throw "Format($($case.Idle), $($case.Bright)) gave '$actual'" }
    if ($null -ne $actual -and $actual.Length -gt 128) { throw 'Message longer than 128 bytes' }
    $checks++
}

# The sender writes to a real pipe server (message mode, inbound), the way the service listens.
$name = 'EcoWattSense.Test.' + [guid]::NewGuid().ToString('N')
$server = [IO.Pipes.NamedPipeServerStream]::new($name, 'In', 1, 'Message', 'Asynchronous', 256, 256)
try {
    $sender = [EcoWattSense.Client.Gui.Viewer.ServicePipeSender]::new($name, 2000)
    $accept = $server.WaitForConnectionAsync()
    $text = $message::Format(7.0, [Nullable[int]]42)
    if (-not $sender.Send($text)) { throw 'Send to a live pipe failed' }
    $accept.Wait(2000) | Out-Null
    $buffer = New-Object byte[] 128
    $count = $server.Read($buffer, 0, 128)
    $received = [Text.Encoding]::ASCII.GetString($buffer, 0, $count)
    if ($received -ne "EWS1 idle=7 bright=42`n") { throw "Server received '$received'" }
    if ($sender.Sent -ne 1 -or $sender.Failed -ne 0) { throw 'Counters wrong after a good send' }
    $checks += 3
} finally { $server.Dispose() }

# A service that never reads (zero-size buffer) must not hold the sender: the write is cancelled at the time limit.
$stuckName = 'EcoWattSense.Test.' + [guid]::NewGuid().ToString('N')
$stuck = [IO.Pipes.NamedPipeServerStream]::new($stuckName, 'In', 1, 'Message', 'Asynchronous')
try {
    $stuckSender = [EcoWattSense.Client.Gui.Viewer.ServicePipeSender]::new($stuckName, 300)
    if ($stuckSender.Send("EWS1 idle=1 bright=-`n")) { throw 'Send to a service that never reads reported success' }
    if ($stuckSender.Failed -ne 1) { throw 'Stuck-service counter wrong' }
    $checks += 2
} finally { $stuck.Dispose() }

# Failure paths: a missing pipe and a message that could not be built are counted, never thrown.
$lonely = [EcoWattSense.Client.Gui.Viewer.ServicePipeSender]::new('EcoWattSense.Test.NoSuchPipe', 100)
if ($lonely.Send("EWS1 idle=1 bright=-`n")) { throw 'Send to a missing pipe reported success' }
if ($lonely.Send($null)) { throw 'Send of null reported success' }
if ($lonely.Failed -ne 2 -or $lonely.Sent -ne 0) { throw 'Failure counters wrong' }
$checks += 3
Write-Output "Passed $checks service-pipe checks."
