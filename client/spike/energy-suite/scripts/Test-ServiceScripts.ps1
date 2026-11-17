<#
.SYNOPSIS
Checks the plans of the service test scripts. Needs no rights and changes nothing.

.DESCRIPTION
Runs Install-ServiceForTest.ps1 and Uninstall-ServiceForTest.ps1 with -DefinitionOnly
and checks the plans against SPEC 2.2, 5.1 and 5.6. Runs Measure-Service.ps1 against a
service that does not exist and expects exit 2. Prints one line per check and exits
with the number of failed checks.

.EXAMPLE
pwsh -NoProfile -File .\Test-ServiceScripts.ps1
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$here = $PSScriptRoot
$installScript = Join-Path $here 'Install-ServiceForTest.ps1'
$uninstallScript = Join-Path $here 'Uninstall-ServiceForTest.ps1'
$measureScript = Join-Path $here 'Measure-Service.ps1'
$fakeExe = 'C:\fake\ews-service.exe'
$fakeData = 'C:\fake\data'

$failed = 0

<#
.SYNOPSIS
Prints one check line and counts a failure.
.PARAMETER Name  What is checked.
.PARAMETER Passed  The result.
#>
function Confirm-Check {
    param([string]$Name, [bool]$Passed)
    $word = if ($Passed) { 'PASS' } else { 'FAIL' }
    Write-Host "$word  $Name"
    if (-not $Passed) { $script:failed++ }
}

<#
.SYNOPSIS
Finds the install-plan action of a kind with a target.
.OUTPUTS
The plan object or $null.
#>
function Find-Action {
    param($Plan, [string]$Kind, [string]$Target)
    return $Plan | Where-Object Kind -eq $Kind | Where-Object Target -eq $Target | Select-Object -First 1
}

<#
.SYNOPSIS
Checks that the ACL action of a data folder holds exactly the expected grants.
.PARAMETER Plan  The install plan.
.PARAMETER Name  Folder name below the data folder.
.PARAMETER Expected  The exact set of grant strings, in any order. Any other grant fails.
#>
function Confirm-Rights {
    param($Plan, [string]$Name, [string[]]$Expected)
    $folder = Join-Path $fakeData $Name
    $acl = Find-Action $Plan 'acl' $folder
    $made = Find-Action $Plan 'folder' $folder
    Confirm-Check "folder $Name is created" ($null -ne $made)
    Confirm-Check "folder $Name has an ACL action" ($null -ne $acl)
    $grants = @($acl.ArgList -like '*:(*')
    # Compare-Object gives nothing when both sets are equal: nothing missing, nothing extra.
    $difference = @(Compare-Object $Expected $grants)
    Confirm-Check "folder $Name rights: exactly the expected grants ($($grants.Count) in total)" ($difference.Count -eq 0)
    Confirm-Check "folder $Name inheritance removed" ($acl.ArgList -contains '/inheritance:r')
}

$sys = '*S-1-5-18:(OI)(CI)F'
$adm = '*S-1-5-32-544:(OI)(CI)F'
$svcM = '*S-1-5-19:(OI)(CI)M'
$svcR = '*S-1-5-19:(OI)(CI)R'
$usrM = '*S-1-5-32-545:(OI)(CI)M'
$usrR = '*S-1-5-32-545:(OI)(CI)R'

$plan = @(& $installScript -ExePath $fakeExe -DataDir $fakeData -DefinitionOnly)
Confirm-Check 'install plan is not empty' ($plan.Count -gt 0)

Confirm-Rights $plan 'scan' @($sys, $adm, $svcM, $usrR)
Confirm-Rights $plan 'calibration' @($sys, $adm, $svcR, $usrM)
Confirm-Rights $plan 'history' @($sys, $adm, $svcM, $usrR)
Confirm-Rights $plan 'status' @($sys, $adm, $svcM, $usrR)
Confirm-Rights $plan 'queue' @($sys, $adm, $svcM)
Confirm-Rights $plan 'config' @($sys, $adm, $svcR)
Confirm-Rights $plan 'logs' @($sys, $adm, $svcM, $usrR)

$client = Find-Action $plan 'acl' (Join-Path $fakeData 'Client')
Confirm-Check 'Client: only an added modify grant for the service' (($client.Data.Mode -eq 'add') -and ($client.ArgList -contains $svcM) -and ($client.ArgList -notcontains '/inheritance:r'))

$root = Find-Action $plan 'acl' $fakeData
Confirm-Check 'data folder: inheritance removed' ($root.ArgList -contains '/inheritance:r')

$create = $plan | Where-Object Kind -eq 'service-create' | Select-Object -First 1
Confirm-Check 'service name' ($create.Data.ServiceName -eq 'EcoWattSenseService')
Confirm-Check 'service display name' ($create.Data.DisplayName -eq 'EcoWattSense Energy Service')
Confirm-Check 'service account is LocalService' ($create.Data.Account -eq 'NT AUTHORITY\LocalService')
Confirm-Check 'service start is delayed-auto' (($create.Data.Start -eq 'delayed-auto') -and ($create.ArgList -contains 'delayed-auto'))
$recovery = $plan | Where-Object Kind -eq 'service-recovery' | Select-Object -First 1
Confirm-Check 'recovery: restart after 60 s, three times' ($recovery.ArgList -contains 'restart/60000/restart/60000/restart/60000')
$ini = $plan | Where-Object Kind -eq 'ini' | Select-Object -First 1
Confirm-Check 'service.ini defaults have server_enabled=0' ($ini.ArgList -contains 'server_enabled=0')
$starts = @($plan | Where-Object Kind -like '*start*' | Where-Object Kind -ne 'service-create')
Confirm-Check 'no start action' ($starts.Count -eq 0)
$orderOk = ($plan[-3].Kind -eq 'service-create') -and ($plan[-2].Kind -eq 'service-recovery') -and ($plan[-1].Kind -eq 'service-description')
Confirm-Check 'service registration is the last step' $orderOk

$systemPlan = @(& $installScript -ExePath $fakeExe -DataDir $fakeData -Account LocalSystem -DefinitionOnly)
$systemCreate = $systemPlan | Where-Object Kind -eq 'service-create' | Select-Object -First 1
Confirm-Check 'LocalSystem: account changes' ($systemCreate.Data.Account -eq 'LocalSystem')
$systemQueue = Find-Action $systemPlan 'acl' (Join-Path $fakeData 'queue')
Confirm-Check 'LocalSystem: rights use *S-1-5-18' ($systemQueue.ArgList -contains $sys)
Confirm-Check 'LocalSystem: no LocalService grant left' (@($systemPlan.ArgList -like '*S-1-5-19*').Count -eq 0)

$scanPlan = @(& $installScript -ExePath $fakeExe -ScanExePath 'C:\fake\ews-scan.exe' -DataDir $fakeData -DefinitionOnly)
Confirm-Check 'ScanExePath adds one copy action' ((@($scanPlan | Where-Object Kind -eq 'copy')).Count -eq 2)

$uninstall = @(& $uninstallScript -DataDir $fakeData -DefinitionOnly)
Confirm-Check 'uninstall: stop, delete, remove program folder' (($uninstall[0].Kind -eq 'service-stop') -and ($uninstall[1].Kind -eq 'service-delete') -and ($uninstall[2].Target -eq 'C:\Program Files\EcoWattSense'))
$dataActions = @($uninstall | Where-Object Target -like "$fakeData*")
Confirm-Check 'uninstall without -RemoveData: no data action' ($dataActions.Count -eq 0)

$uninstallData = @(& $uninstallScript -DataDir $fakeData -RemoveData -DefinitionOnly)
$dataRemovals = @($uninstallData | Where-Object Target -like "$fakeData*")
$clientNamed = @($uninstallData | Where-Object Target -like '*Client*')
Confirm-Check 'uninstall with -RemoveData: seven sub-folders' ($dataRemovals.Count -eq 7)
Confirm-Check 'uninstall with -RemoveData: never names Client' ($clientNamed.Count -eq 0)

& pwsh -NoProfile -File $measureScript -ServiceName 'NoSuchEcoWattService' | Out-Null
Confirm-Check 'Measure-Service: missing service gives exit 2' ($LASTEXITCODE -eq 2)

Write-Host ''
Write-Host "Failed checks: $failed"
exit $failed
