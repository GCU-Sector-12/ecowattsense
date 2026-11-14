# Run with PowerShell 7 (.NET 10): pwsh -File ./tests/Test-ViewerMode.ps1
# Checks the viewer-mode choice (SPEC 9) with fake service lookups, the state words, the SCM state mapping,
# and two live probes of the real service control manager (a service that cannot exist, and EventLog).
$ErrorActionPreference = 'Stop'
Add-Type -Path (Join-Path $PSScriptRoot '../../EcoWattSense.Client.Gui/Viewer/ViewerMode.cs')
$mode = [EcoWattSense.Client.Gui.Viewer.ViewerMode]
$state = [EcoWattSense.Client.Gui.Viewer.ServiceState]
$scm = [EcoWattSense.Client.Gui.Viewer.ScmServiceLookup]
$checks = 0

# Fake lookups: not installed and unknown keep the classic mode; installed in any state is the viewer.
foreach ($case in @(
    @{ State = 'NotInstalled'; Viewer = $false },
    @{ State = 'Unknown'; Viewer = $false },
    @{ State = 'Running'; Viewer = $true },
    @{ State = 'Stopped'; Viewer = $true },
    @{ State = 'Starting'; Viewer = $true },
    @{ State = 'Stopping'; Viewer = $true },
    @{ State = 'Paused'; Viewer = $true }
)) {
    $value = [Enum]::Parse($state, $case.State)
    $lookup = [Func[EcoWattSense.Client.Gui.Viewer.ServiceState]]{ $value }.GetNewClosure()
    if ($mode::IsViewer($lookup) -ne $case.Viewer) { throw "Wrong mode for $($case.State)" }
    $checks++
}

foreach ($case in @(
    @{ State = 'NotInstalled'; Word = 'Not installed' }, @{ State = 'Stopped'; Word = 'Stopped' },
    @{ State = 'Running'; Word = 'Running' }, @{ State = 'Unknown'; Word = 'Unknown' }
)) {
    if ($mode::Word([Enum]::Parse($state, $case.State)) -ne $case.Word) { throw "Wrong word for $($case.State)" }
    $checks++
}

# SERVICE_STATUS numbers 1 to 7, and a number outside the range.
foreach ($case in @(
    @{ N = 1; S = 'Stopped' }, @{ N = 2; S = 'Starting' }, @{ N = 3; S = 'Stopping' }, @{ N = 4; S = 'Running' },
    @{ N = 5; S = 'Starting' }, @{ N = 6; S = 'Paused' }, @{ N = 7; S = 'Paused' }, @{ N = 0; S = 'Unknown' },
    @{ N = 99; S = 'Unknown' }
)) {
    if ($scm::MapState([uint32]$case.N).ToString() -ne $case.S) { throw "Wrong state mapping for $($case.N)" }
    $checks++
}

# Live probes: a standard user may query a service state without admin rights.
if ($scm::Query('EcoWattSense_NoSuchService_x9').ToString() -ne 'NotInstalled') { throw 'Missing service not reported as NotInstalled' }
$checks++
if ($scm::Query('EventLog').ToString() -ne 'Running') { throw 'EventLog not reported as Running' }
$checks++
Write-Output "Passed $checks viewer-mode checks."
