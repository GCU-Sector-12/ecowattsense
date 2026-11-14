<#
.SYNOPSIS
Installs the EcoWattSense Windows service on this PC for a manual test.

.DESCRIPTION
A test tool, not the final installer. A person runs it from an elevated PowerShell 7
prompt (needs administrator rights). It copies the program files, creates the data
folder with the rights of SPEC 2.2, writes a default config\service.ini (only when none
exists) and registers the service (SPEC 5.1). It does NOT start the service.

Exit codes: 0 done, 2 not elevated, 3 service already exists, 4 a step failed
(the message says which earlier steps were already done).

Rights use well-known SIDs, so they work on every Windows language.
The server key is never read, printed or logged by this script.

.PARAMETER ExePath
Path to ews-service.exe. Required.

.PARAMETER ScanExePath
Optional path to ews-scan.exe, copied next to the service.

.PARAMETER Account
LocalService (default) or LocalSystem.

.PARAMETER DataDir
Data folder. Default C:\ProgramData\EcoWattSense.

.PARAMETER DefinitionOnly
Change nothing. Return the ordered plan: one object per action (Kind, Target, Detail,
ArgList, Data). Needs no rights. Used by Test-ServiceScripts.ps1.

.EXAMPLE
pwsh -File .\Install-ServiceForTest.ps1 -ExePath C:\build\ews-service.exe
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath,

    [string]$ScanExePath = '',

    [ValidateSet('LocalService', 'LocalSystem')]
    [string]$Account = 'LocalService',

    [string]$DataDir = 'C:\ProgramData\EcoWattSense',

    [switch]$DefinitionOnly
)

$ErrorActionPreference = 'Stop'
# Standard mode passes embedded quotes to sc.exe and icacls correctly.
$PSNativeCommandArgumentPassing = 'Standard'

$ServiceName = 'EcoWattSenseService'
$DisplayName = 'EcoWattSense Energy Service'
$ProgramDir = 'C:\Program Files\EcoWattSense'

# Well-known SIDs (SPEC 2.2).
$SidSystem = '*S-1-5-18'
$SidAdmins = '*S-1-5-32-544'
$SidUsers = '*S-1-5-32-545'
$SidLocalService = '*S-1-5-19'

# Service account right and Users right for each data sub-folder (SPEC 2.2 table).
# F full, M modify, R read, N none.
$FolderRights = [ordered]@{
    'scan'        = @{ Service = 'M'; Users = 'R' }
    'calibration' = @{ Service = 'R'; Users = 'M' }
    'history'     = @{ Service = 'M'; Users = 'R' }
    'status'      = @{ Service = 'M'; Users = 'R' }
    'queue'       = @{ Service = 'M'; Users = 'N' }
    'config'      = @{ Service = 'R'; Users = 'N' }
    'logs'        = @{ Service = 'M'; Users = 'R' }
}

# SPEC 5.6 defaults. The server stays off until a person turns it on.
$IniDefaults = @(
    'server_enabled=0',
    'server_url=',
    'interval_seconds=300',
    'idle_threshold_seconds=600',
    'unused_threshold_seconds=3600',
    'device_type=laptop'
)

<#
.SYNOPSIS
Builds one plan object.
.PARAMETER Kind  Action kind, for example folder, acl, copy.
.PARAMETER Target  The path or service the action works on.
.PARAMETER Detail  Short text for the person.
.PARAMETER ArgList  Arguments for the external command, if any.
.PARAMETER Data  Extra facts the handler or a test needs.
.OUTPUTS
PSCustomObject.
#>
function New-Action {
    param([string]$Kind, [string]$Target, [string]$Detail, [string[]]$ArgList = @(), [hashtable]$Data = @{})
    return [pscustomobject]@{ Kind = $Kind; Target = $Target; Detail = $Detail; ArgList = $ArgList; Data = $Data }
}

<#
.SYNOPSIS
Formats one icacls grant such as *S-1-5-19:(OI)(CI)M.
.PARAMETER Sid  The SID with a leading star.
.PARAMETER Right  F, M or R.
.OUTPUTS
String.
#>
function Format-Grant {
    param([string]$Sid, [string]$Right)
    return "${Sid}:(OI)(CI)$Right"
}

<#
.SYNOPSIS
Builds the icacls argument list that resets a folder to an exact set of rights.
.PARAMETER ServiceSid  SID of the service account.
.PARAMETER ServiceRight  F, M, R or N.
.PARAMETER UsersRight  F, M, R or N.
.OUTPUTS
String array: inheritance removed, then one /grant:r per entry. N means no entry.
SYSTEM is skipped as service account because it already has full rights.
#>
function Get-FolderAclArgs {
    param([string]$ServiceSid, [string]$ServiceRight, [string]$UsersRight)
    $grants = [System.Collections.Generic.List[string]]::new()
    $grants.Add((Format-Grant $SidSystem 'F'))
    $grants.Add((Format-Grant $SidAdmins 'F'))
    $addService = ($ServiceRight -ne 'N') -and ($ServiceSid -ne $SidSystem)
    if ($addService) { $grants.Add((Format-Grant $ServiceSid $ServiceRight)) }
    if ($UsersRight -ne 'N') { $grants.Add((Format-Grant $SidUsers $UsersRight)) }
    $icaclsArgs = [System.Collections.Generic.List[string]]::new()
    $icaclsArgs.Add('/inheritance:r')
    foreach ($grant in $grants) { $icaclsArgs.Add('/grant:r'); $icaclsArgs.Add($grant) }
    return $icaclsArgs.ToArray()
}

<#
.SYNOPSIS
Builds the ordered plan of everything this script does.
.PARAMETER Exe  Full path of ews-service.exe.
.PARAMETER ScanExe  Full path of ews-scan.exe or empty.
.PARAMETER Data  Full data folder.
.PARAMETER ServiceSid  SID of the service account.
.PARAMETER Obj  Account name for sc.exe obj=.
.OUTPUTS
Array of plan objects. Never contains a start action.
#>
function New-InstallPlan {
    param([string]$Exe, [string]$ScanExe, [string]$Data, [string]$ServiceSid, [string]$Obj)
    $plan = [System.Collections.Generic.List[object]]::new()
    $installedExe = Join-Path $ProgramDir (Split-Path -Leaf $Exe)
    $plan.Add((New-Action 'folder' $ProgramDir 'create the program folder'))
    $plan.Add((New-Action 'copy' $installedExe "copy from $Exe" @($Exe, $installedExe)))
    if ($ScanExe -ne '') {
        $installedScan = Join-Path $ProgramDir (Split-Path -Leaf $ScanExe)
        $plan.Add((New-Action 'copy' $installedScan "copy from $ScanExe" @($ScanExe, $installedScan)))
    }
    $plan.Add((New-Action 'folder' $Data 'create the data folder'))
    # The folder itself: Users and the service may only traverse it (no inheritance flags).
    $rootArgs = @('/inheritance:r', '/grant:r', (Format-Grant $SidSystem 'F'), '/grant:r', (Format-Grant $SidAdmins 'F'),
        '/grant:r', "${SidUsers}:(RX)", '/grant:r', "${ServiceSid}:(RX)")
    $plan.Add((New-Action 'acl' $Data 'data folder: inheritance removed, SYSTEM and Administrators full, Users and service traverse only' $rootArgs))
    $clientFolder = Join-Path $Data 'Client'
    $plan.Add((New-Action 'folder-client' $clientFolder 'create only when missing; a new folder also gets Users modify'))
    $plan.Add((New-Action 'acl' $clientFolder 'Client keeps its rights; the service account is added with modify' @('/grant', (Format-Grant $ServiceSid 'M')) @{ Mode = 'add' }))
    foreach ($name in $FolderRights.Keys) {
        $folder = Join-Path $Data $name
        $rights = $FolderRights[$name]
        $plan.Add((New-Action 'folder' $folder "create $name"))
        $aclArgs = Get-FolderAclArgs $ServiceSid $rights.Service $rights.Users
        $plan.Add((New-Action 'acl' $folder "service $($rights.Service), Users $($rights.Users), SYSTEM and Administrators F" $aclArgs @{ Mode = 'reset' }))
    }
    $ini = Join-Path (Join-Path $Data 'config') 'service.ini'
    $plan.Add((New-Action 'ini' $ini 'write defaults (server_enabled=0) only when the file does not exist' $IniDefaults))
    $binPath = '"' + $installedExe + '"'
    $createArgs = @('create', $ServiceName, 'binPath=', $binPath, 'start=', 'delayed-auto', 'obj=', $Obj, 'DisplayName=', $DisplayName)
    $createData = @{ ServiceName = $ServiceName; Account = $Obj; Start = 'delayed-auto'; DisplayName = $DisplayName }
    $plan.Add((New-Action 'service-create' $ServiceName "account $Obj, automatic delayed start" $createArgs $createData))
    $recoveryArgs = @('failure', $ServiceName, 'reset=', '86400', 'actions=', 'restart/60000/restart/60000/restart/60000')
    $plan.Add((New-Action 'service-recovery' $ServiceName 'restart after 60 seconds, three times' $recoveryArgs @{ Delay = '60000'; Count = '3' }))
    $plan.Add((New-Action 'service-description' $ServiceName 'set the description' @('description', $ServiceName, $DisplayName)))
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
Tells whether the service is registered.
.OUTPUTS
Boolean. sc.exe query exits with 1060 when the service does not exist.
#>
function Test-ServiceExists {
    sc.exe query $ServiceName | Out-Null
    return ($LASTEXITCODE -ne 1060)
}

<#
.SYNOPSIS
Prints an error, says which steps were already done and ends the script.
.PARAMETER Code  Exit code.
.PARAMETER Message  What went wrong.
.PARAMETER Done  Descriptions of the steps already finished.
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
Runs an external command quietly.
.PARAMETER File  The program.
.PARAMETER Params  Its arguments.
.OUTPUTS
The exit code.
#>
function Invoke-Native {
    param([string]$File, [string[]]$Params)
    & $File @Params | Out-Null
    return $LASTEXITCODE
}

<#
.SYNOPSIS
Handler: creates a folder.
.OUTPUTS
Error text or $null.
#>
function Step-Folder {
    param($Action)
    try { New-Item -ItemType Directory -Force -Path $Action.Target | Out-Null }
    catch { return "cannot create folder $($Action.Target): $($_.Exception.Message)" }
    return $null
}

<#
.SYNOPSIS
Handler: creates the Client folder when missing. A new one gets Users modify, because
inheritance is off on the data folder. An existing one is left untouched.
.OUTPUTS
Error text or $null.
#>
function Step-FolderClient {
    param($Action)
    if (Test-Path -LiteralPath $Action.Target) { return $null }
    $problem = Step-Folder $Action
    if ($problem) { return $problem }
    $code = Invoke-Native 'icacls.exe' @($Action.Target, '/grant', (Format-Grant $SidUsers 'M'))
    if ($code -ne 0) { return "icacls failed on $($Action.Target) (exit $code)" }
    return $null
}

<#
.SYNOPSIS
Handler: copies a program file.
.OUTPUTS
Error text or $null.
#>
function Step-Copy {
    param($Action)
    try { Copy-Item -LiteralPath $Action.ArgList[0] -Destination $Action.ArgList[1] -Force }
    catch { return "cannot copy $($Action.ArgList[0]): $($_.Exception.Message)" }
    return $null
}

<#
.SYNOPSIS
Handler: runs icacls with the planned arguments.
.OUTPUTS
Error text or $null.
#>
function Step-Acl {
    param($Action)
    $code = Invoke-Native 'icacls.exe' (@($Action.Target) + $Action.ArgList)
    if ($code -ne 0) { return "icacls failed on $($Action.Target) (exit $code)" }
    return $null
}

<#
.SYNOPSIS
Handler: writes service.ini with the defaults when none exists.
.OUTPUTS
Error text or $null.
#>
function Step-Ini {
    param($Action)
    if (Test-Path -LiteralPath $Action.Target) { return $null }
    try { Set-Content -LiteralPath $Action.Target -Value $Action.ArgList -Encoding ascii }
    catch { return "cannot write $($Action.Target): $($_.Exception.Message)" }
    return $null
}

<#
.SYNOPSIS
Handler: runs sc.exe with the planned arguments.
.OUTPUTS
Error text or $null.
#>
function Step-Sc {
    param($Action)
    $code = Invoke-Native 'sc.exe' $Action.ArgList
    if ($code -ne 0) { return "sc.exe $($Action.ArgList[0]) failed (exit $code)" }
    return $null
}

$Handlers = @{
    'folder'              = ${function:Step-Folder}
    'folder-client'       = ${function:Step-FolderClient}
    'copy'                = ${function:Step-Copy}
    'acl'                 = ${function:Step-Acl}
    'ini'                 = ${function:Step-Ini}
    'service-create'      = ${function:Step-Sc}
    'service-recovery'    = ${function:Step-Sc}
    'service-description' = ${function:Step-Sc}
}

<#
.SYNOPSIS
Runs one plan action. Ends the script with exit 4 when it fails.
.PARAMETER Action  The plan object.
.PARAMETER Done  List that collects finished steps.
#>
function Invoke-Step {
    param($Action, $Done)
    Write-Host ("[{0}] {1}: {2}" -f $Action.Kind, $Action.Target, $Action.Detail)
    $problem = & $Handlers[$Action.Kind] $Action
    if ($problem) { Stop-WithFailure 4 $problem $Done.ToArray() }
    $Done.Add("$($Action.Kind) $($Action.Target)")
}

$resolve = $ExecutionContext.SessionState.Path
$exeFull = $resolve.GetUnresolvedProviderPathFromPSPath($ExePath)
$scanFull = ''
if ($ScanExePath -ne '') { $scanFull = $resolve.GetUnresolvedProviderPathFromPSPath($ScanExePath) }
$dataFull = $resolve.GetUnresolvedProviderPathFromPSPath($DataDir)
$serviceSid = if ($Account -eq 'LocalSystem') { $SidSystem } else { $SidLocalService }
$obj = if ($Account -eq 'LocalSystem') { 'LocalSystem' } else { 'NT AUTHORITY\LocalService' }

$plan = New-InstallPlan $exeFull $scanFull $dataFull $serviceSid $obj
if ($DefinitionOnly) { return $plan }

if (-not (Test-Elevated)) { Stop-WithFailure 2 'Run this script from an elevated PowerShell (Run as administrator).' }
if (Test-ServiceExists) { Stop-WithFailure 3 "The service $ServiceName already exists. Run Uninstall-ServiceForTest.ps1 first." }
if (-not (Test-Path -LiteralPath $exeFull -PathType Leaf)) { Stop-WithFailure 4 "ews-service.exe not found: $exeFull" }
if ($scanFull -ne '' -and -not (Test-Path -LiteralPath $scanFull -PathType Leaf)) { Stop-WithFailure 4 "ews-scan.exe not found: $scanFull" }

$done = [System.Collections.Generic.List[string]]::new()
foreach ($action in $plan) { Invoke-Step $action $done }

$installed = Join-Path $ProgramDir (Split-Path -Leaf $exeFull)
Write-Host ''
Write-Host 'The service is installed but NOT started. Next:'
Write-Host "  1. Optional, to send reports: & '$installed' --configure"
Write-Host '     then type the lines  url=<server url>  and  key=<server key>,  then Ctrl+Z and Enter.'
Write-Host '     (The key is typed at the prompt, never on the command line.) Then set server_enabled=1 in config\service.ini.'
Write-Host "  2. Start it: sc.exe start $ServiceName"
exit 0
