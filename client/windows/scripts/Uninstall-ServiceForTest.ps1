<#
.SYNOPSIS
Removes the EcoWattSense Windows service installed by Install-ServiceForTest.ps1.

.DESCRIPTION
A test tool. Run it from an elevated PowerShell 7 prompt (needs administrator rights).
It stops the service when it runs (waits up to 30 seconds), deletes it and removes the
program folder. The data folder stays, unless -RemoveData is given. Then only the
sub-folders scan, calibration, history, status, queue, config and logs are removed.
The Client folder belongs to the existing client and is never touched.
A missing service or folder is not an error.

Exit codes: 0 done, 2 not elevated, 4 a step failed (the message lists the steps done).

.PARAMETER RemoveData
Also remove the seven service sub-folders of the data folder.

.PARAMETER DataDir
Data folder. Default C:\ProgramData\EcoWattSense.

.PARAMETER DefinitionOnly
Change nothing. Return the ordered plan (Kind, Target, Detail). Needs no rights.

.EXAMPLE
pwsh -File .\Uninstall-ServiceForTest.ps1 -RemoveData
#>
[CmdletBinding()]
param(
    [switch]$RemoveData,

    [string]$DataDir = 'C:\ProgramData\EcoWattSense',

    [switch]$DefinitionOnly
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandArgumentPassing = 'Standard'

$ServiceName = 'EcoWattSenseService'
$ProgramDir = 'C:\Program Files\EcoWattSense'
$StopLimitSeconds = 30
# Never includes Client.
$DataSubFolders = @('scan', 'calibration', 'history', 'status', 'queue', 'config', 'logs')

<#
.SYNOPSIS
Builds one plan object.
.OUTPUTS
PSCustomObject with Kind, Target, Detail.
#>
function New-Action {
    param([string]$Kind, [string]$Target, [string]$Detail)
    return [pscustomobject]@{ Kind = $Kind; Target = $Target; Detail = $Detail }
}

<#
.SYNOPSIS
Builds the ordered plan.
.PARAMETER Data  Full data folder.
.PARAMETER WithData  True to include the removal of the data sub-folders.
.OUTPUTS
Array of plan objects.
#>
function New-UninstallPlan {
    param([string]$Data, [bool]$WithData)
    $plan = [System.Collections.Generic.List[object]]::new()
    $plan.Add((New-Action 'service-stop' $ServiceName "stop when running, wait up to $StopLimitSeconds seconds"))
    $plan.Add((New-Action 'service-delete' $ServiceName 'delete the service when it exists'))
    $plan.Add((New-Action 'folder-remove' $ProgramDir 'remove the program folder when it exists'))
    if (-not $WithData) { return $plan.ToArray() }
    foreach ($name in $DataSubFolders) {
        $plan.Add((New-Action 'folder-remove' (Join-Path $Data $name) "remove $name"))
    }
    return $plan.ToArray()
}

<#
.SYNOPSIS
Tells whether this PowerShell runs with administrator rights.
.OUTPUTS
Boolean.
#>
function Test-Elevated {
    $principal = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

<#
.SYNOPSIS
Reads the state of the service through sc.exe query.
.OUTPUTS
'MISSING' when the service does not exist (exit 1060), 'STOPPED' when the state
code is 1, otherwise 'OTHER'. The numeric code is used because it is not translated.
#>
function Get-ServiceState {
    $text = sc.exe query $ServiceName
    if ($LASTEXITCODE -eq 1060) { return 'MISSING' }
    $match = [regex]::Match(($text -join "`n"), 'STATE\s*:\s*(\d+)')
    if ($match.Groups[1].Value -eq '1') { return 'STOPPED' }
    return 'OTHER'
}

<#
.SYNOPSIS
Waits until the service is stopped or gone, with a growing pause (1, 2, 4, 8 seconds).
.PARAMETER Limit  Longest wait in seconds.
.OUTPUTS
Boolean: true when stopped or gone.
#>
function Wait-ServiceStopped {
    param([int]$Limit)
    $pause = 1
    $waited = 0
    while ($waited -lt $Limit -and (Get-ServiceState) -eq 'OTHER') {
        $sleepFor = [Math]::Min($pause, $Limit - $waited)
        Start-Sleep -Seconds $sleepFor
        $waited += $sleepFor
        $pause = [Math]::Min($pause * 2, 8)
    }
    return ((Get-ServiceState) -ne 'OTHER')
}

<#
.SYNOPSIS
Prints an error, says which steps were already done and ends the script.
#>
function Stop-WithFailure {
    param([int]$Code, [string]$Message, $Done = @())
    Write-Host "ERROR: $Message" -ForegroundColor Red
    $doneText = if ($Done.Count -eq 0) { 'none' } else { $Done -join '; ' }
    Write-Host "Steps already done: $doneText"
    exit $Code
}

<#
.SYNOPSIS
Handler: stops the service and waits. A missing or stopped service is fine.
.OUTPUTS
Error text or $null.
#>
function Step-ServiceStop {
    param($Action)
    if ((Get-ServiceState) -ne 'OTHER') { return $null }
    sc.exe stop $ServiceName | Out-Null
    # 1062 means the service was not started: fine.
    if ($LASTEXITCODE -notin 0, 1062) { return "sc.exe stop failed (exit $LASTEXITCODE)" }
    if (-not (Wait-ServiceStopped $StopLimitSeconds)) { return "the service did not stop within $StopLimitSeconds seconds" }
    return $null
}

<#
.SYNOPSIS
Handler: deletes the service. A missing service is fine.
.OUTPUTS
Error text or $null.
#>
function Step-ServiceDelete {
    param($Action)
    if ((Get-ServiceState) -eq 'MISSING') { return $null }
    sc.exe delete $ServiceName | Out-Null
    if ($LASTEXITCODE -ne 0) { return "sc.exe delete failed (exit $LASTEXITCODE)" }
    return $null
}

<#
.SYNOPSIS
Handler: removes a folder with its content. A missing folder is fine.
A locked file gives a clear message.
.OUTPUTS
Error text or $null.
#>
function Step-FolderRemove {
    param($Action)
    if (-not (Test-Path -LiteralPath $Action.Target)) { return $null }
    try { Remove-Item -LiteralPath $Action.Target -Recurse -Force }
    catch { return "cannot remove $($Action.Target), a file may be locked: $($_.Exception.Message)" }
    return $null
}

$Handlers = @{
    'service-stop'   = ${function:Step-ServiceStop}
    'service-delete' = ${function:Step-ServiceDelete}
    'folder-remove'  = ${function:Step-FolderRemove}
}

<#
.SYNOPSIS
Runs one plan action. Ends the script with exit 4 when it fails.
#>
function Invoke-Step {
    param($Action, $Done)
    Write-Host ("[{0}] {1}: {2}" -f $Action.Kind, $Action.Target, $Action.Detail)
    $problem = & $Handlers[$Action.Kind] $Action
    if ($problem) { Stop-WithFailure 4 $problem $Done.ToArray() }
    $Done.Add("$($Action.Kind) $($Action.Target)")
}

$dataFull = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($DataDir)
$plan = New-UninstallPlan $dataFull ([bool]$RemoveData)
if ($DefinitionOnly) { return $plan }

if (-not (Test-Elevated)) { Stop-WithFailure 2 'Run this script from an elevated PowerShell (Run as administrator).' }

$done = [System.Collections.Generic.List[string]]::new()
foreach ($action in $plan) { Invoke-Step $action $done }
Write-Host 'Done.'
exit 0
