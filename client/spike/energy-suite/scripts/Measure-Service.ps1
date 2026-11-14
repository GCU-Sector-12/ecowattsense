<#
.SYNOPSIS
Measures what the EcoWattSense service costs while it runs.

.DESCRIPTION
A test tool. No administrator rights needed (the data comes from WMI). It reads the
service process at the start and at the end of a wait: working set, private bytes,
handle count, thread count and total processor time (user + kernel). It waits once
(one sleep, no polling), then prints both snapshots, the processor seconds used, that
as a percent of one core and of all cores, and the file size of the program.

Exit codes: 0 done, 2 the service is missing or not running,
3 the process ended (or was replaced) during the wait.

.PARAMETER Minutes
How long to wait between the snapshots. Default 10.

.PARAMETER ServiceName
Default EcoWattSenseService.

.EXAMPLE
pwsh -File .\Measure-Service.ps1 -Minutes 10
#>
[CmdletBinding()]
param(
    [double]$Minutes = 10,

    [string]$ServiceName = 'EcoWattSenseService'
)

$ErrorActionPreference = 'Stop'

<#
.SYNOPSIS
Reads the process id and program path of the service.
.OUTPUTS
Service object from Win32_Service, or $null when the service does not exist.
#>
function Get-ServiceInfo {
    return Get-CimInstance -ClassName Win32_Service -Filter "Name='$ServiceName'"
}

<#
.SYNOPSIS
Takes one snapshot of a process.
.PARAMETER ProcessId  The process id.
.OUTPUTS
PSCustomObject, or $null when the process does not exist. WMI times are in 100 ns
units and are converted to seconds.
#>
function Get-Snapshot {
    param([uint32]$ProcessId)
    $proc = Get-CimInstance -ClassName Win32_Process -Filter "ProcessId=$ProcessId"
    if ($null -eq $proc) { return $null }
    return [pscustomobject]@{
        At           = Get-Date
        Created      = $proc.CreationDate
        WorkingSetMB = [math]::Round($proc.WorkingSetSize / 1MB, 2)
        PrivateMB    = [math]::Round($proc.PrivatePageCount / 1MB, 2)
        Handles      = $proc.HandleCount
        Threads      = $proc.ThreadCount
        CpuSeconds   = ($proc.UserModeTime + $proc.KernelModeTime) / 1e7
    }
}

<#
.SYNOPSIS
Extracts the program file path from a service PathName (quoted or not, with arguments).
.PARAMETER PathName  Value of Win32_Service.PathName.
.OUTPUTS
The path string.
#>
function Get-ProgramPath {
    param([string]$PathName)
    $match = [regex]::Match($PathName, '^\s*(?:"([^"]+)"|(\S+))')
    return ($match.Groups[1].Value + $match.Groups[2].Value)
}

<#
.SYNOPSIS
Prints an error and ends the script.
#>
function Stop-WithFailure {
    param([int]$Code, [string]$Message)
    Write-Host "ERROR: $Message" -ForegroundColor Red
    exit $Code
}

$service = Get-ServiceInfo
if ($null -eq $service) { Stop-WithFailure 2 "Service $ServiceName does not exist." }
if ($service.ProcessId -eq 0) { Stop-WithFailure 2 "Service $ServiceName is not running." }

$first = Get-Snapshot $service.ProcessId
if ($null -eq $first) { Stop-WithFailure 2 "Process $($service.ProcessId) of $ServiceName is gone." }

Write-Host "Measuring process $($service.ProcessId) for $Minutes minute(s)..."
Start-Sleep -Seconds ([int]($Minutes * 60))

$second = Get-Snapshot $service.ProcessId
if ($null -eq $second) { Stop-WithFailure 3 'The service process ended during the wait.' }
if ($second.Created -ne $first.Created) { Stop-WithFailure 3 'The service process was replaced during the wait (restart).' }

$wallSeconds = ($second.At - $first.At).TotalSeconds
$cpuUsed = $second.CpuSeconds - $first.CpuSeconds
$oneCore = 100 * $cpuUsed / $wallSeconds
$allCores = $oneCore / [Environment]::ProcessorCount
$programPath = Get-ProgramPath $service.PathName
$programSize = (Get-Item -LiteralPath $programPath).Length

Write-Host ''
Write-Host 'Start:'
$first | Format-List WorkingSetMB, PrivateMB, Handles, Threads, CpuSeconds | Out-Host
Write-Host 'End:'
$second | Format-List WorkingSetMB, PrivateMB, Handles, Threads, CpuSeconds | Out-Host
Write-Host ('Wall time:            {0:N1} s' -f $wallSeconds)
Write-Host ('Processor time used:  {0:N3} s' -f $cpuUsed)
Write-Host ('Percent of one core:  {0:N3} %' -f $oneCore)
Write-Host ('Percent of all cores: {0:N3} % ({1} logical cores)' -f $allCores, [Environment]::ProcessorCount)
Write-Host ('Program file:         {0} ({1:N0} bytes)' -f $programPath, $programSize)
exit 0
