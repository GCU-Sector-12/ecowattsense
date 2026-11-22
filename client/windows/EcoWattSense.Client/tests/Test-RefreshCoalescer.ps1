# Run with PowerShell 7 (.NET 10): pwsh -File ./tests/Test-RefreshCoalescer.ps1
# Compiles the real RefreshCoalescer (no WPF) with a plain queue standing in for the dispatcher, and checks that a burst of
# requests gives one run, that a request during a run queues the next one, and that the refresh with a missing history
# folder gives an empty result and no error (the viewer then shows an empty chart and "unknown" cards).
$ErrorActionPreference = 'Stop'
$gui = Join-Path $PSScriptRoot '../../EcoWattSense.Client.Gui'
Add-Type -Path @((Join-Path $gui 'Viewer/RefreshCoalescer.cs'), (Join-Path $gui 'History/HistoryRow.cs'), (Join-Path $gui 'History/HistoryReader.cs'))
$checks = 0
function Check($ok, $what) { if (!$ok) { throw "Failed: $what" }; $script:checks++ }

$queue = [System.Collections.Generic.Queue[Action]]::new()
$state = @{ Runs = 0 }
$post = [Action[Action]]{ param($a) $queue.Enqueue($a) }.GetNewClosure()
$work = [Action]{ $state.Runs++ }.GetNewClosure()
$coalescer = [EcoWattSense.Client.Gui.Viewer.RefreshCoalescer]::new($post, $work)

# Five requests before the dispatcher runs: one queued item, one run.
1..5 | ForEach-Object { $coalescer.Request() }
Check ($queue.Count -eq 1) 'burst queues one item'
$queue.Dequeue().Invoke()
Check ($state.Runs -eq 1 -and $queue.Count -eq 0) 'burst gives one run'

# After the run, a new request queues a new run.
$coalescer.Request()
Check ($queue.Count -eq 1) 'request after a run queues again'
$queue.Dequeue().Invoke()
Check ($state.Runs -eq 2) 'second run'

# A request made while the work runs is not lost: it queues the next run.
$holder = @{}
$work2 = [Action]{ $state.Runs++; $holder.Inner.Request() }.GetNewClosure()
$holder.Inner = [EcoWattSense.Client.Gui.Viewer.RefreshCoalescer]::new($post, $work2)
$holder.Inner.Request()
$queue.Dequeue().Invoke()
Check ($queue.Count -eq 1) 'request during the work queues the next run'
$queue.Clear()

# Requests from many threads at once still give one queued item.
# The post and work delegates are compiled C#, because a script block cannot run on several threads at once.
Add-Type -TypeDefinition '
public sealed class BurstProbe {
    public int Posted;
    public void Post(System.Action a) { System.Threading.Interlocked.Increment(ref Posted); }
    public void Work() { }
}'
$probe = [BurstProbe]::new()
$parallel = [EcoWattSense.Client.Gui.Viewer.RefreshCoalescer]::new([Action[Action]]$probe.Post, [Action]$probe.Work)
1..50 | ForEach-Object -Parallel { $c = $using:parallel; $c.Request() } -ThrottleLimit 8
Check ($probe.Posted -eq 1) 'parallel burst queues one item'

# Refresh with a missing history folder: no rows, no error.
$missing = Join-Path ([IO.Path]::GetTempPath()) ('EcoWattSense-NoHistory-' + [guid]::NewGuid().ToString('N'))
$result = [EcoWattSense.Client.Gui.History.HistoryReader]::Read($missing, [datetime]::UtcNow)
Check ($result.Rows.Count -eq 0 -and $result.FilesRead -eq 0 -and $result.FilesUnreadable -eq 0) 'missing folder gives an empty result'

# Refresh with a locked file: counted as unreadable, no rows, no error.
New-Item -ItemType Directory $missing | Out-Null
try {
    $now = [datetime]::UtcNow
    $path = Join-Path $missing ($now.ToString('yyyy-MM-dd') + '.csv')
    $lock = [IO.File]::Open($path, 'Create', 'ReadWrite', 'None')
    $result = [EcoWattSense.Client.Gui.History.HistoryReader]::Read($missing, $now)
    Check ($result.Rows.Count -eq 0 -and $result.FilesUnreadable -eq 1) 'locked file gives an empty result'
    $lock.Dispose()
} finally {
    if ($lock) { $lock.Dispose() }
    Remove-Item $missing -Recurse -Force
}
Write-Output "Passed $checks refresh-coalescer checks."
