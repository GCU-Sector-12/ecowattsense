# Checks the autostart scripts in client/windows/scripts for Windows Client developers.
# Install-Autostart.ps1 is run with -DefinitionOnly, to check the definition before testing a temporary registration.
# The scripts are checked in PowerShell 7 and, when it is installed, in Windows PowerShell 5.1.
$ErrorActionPreference = 'Stop'

$scripts = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../scripts'))
$install = Join-Path $scripts 'Install-Autostart.ps1'
$uninstall = Join-Path $scripts 'Uninstall-Autostart.ps1'
$user = [Security.Principal.WindowsIdentity]::GetCurrent().Name
$taskName = 'EcoWattSense-AutostartTest-' + [guid]::NewGuid().ToString('N')

$engines = @((Get-Process -Id $PID).Path)
$windowsPowerShell = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
if (Test-Path -LiteralPath $windowsPowerShell) { $engines += $windowsPowerShell }
else { Write-Output 'Windows PowerShell 5.1 not found; checking PowerShell 7 only.' }

<#
.SYNOPSIS
Checks the repetition XML saved for the named task. Throws if the trigger has the wrong interval or a duration.
#>
function Assert-RegisteredRepetition {
    param([string]$TaskName)

    [xml]$definition = Export-ScheduledTask -TaskName $TaskName
    $ns = [System.Xml.XmlNamespaceManager]::new($definition.NameTable)
    $ns.AddNamespace('task', 'http://schemas.microsoft.com/windows/2004/02/mit/task')
    $repetition = $definition.SelectSingleNode('/task:Task/task:Triggers/task:LogonTrigger/task:Repetition', $ns)
    if ($null -eq $repetition) { throw 'The registered logon trigger has no repetition pattern' }
    if ($repetition.SelectSingleNode('task:Interval', $ns).InnerText -ne 'PT5M') { throw 'The registered task does not repeat every five minutes' }
    if ($null -ne $repetition.SelectSingleNode('task:Duration', $ns)) { throw 'The registered task has a repetition duration' }
}

<#
.SYNOPSIS
Reads the named task and checks its enabled flag. Throws on a mismatch.
#>
function Assert-TaskEnabled {
    param([string]$TaskName, [bool]$Enabled)

    $task = Get-ScheduledTask -TaskName $TaskName
    if ($task.Settings.Enabled -ne $Enabled) { throw "Expected task Enabled to be $Enabled" }
}

<#
.SYNOPSIS
Checks registration and the enable/disable commands on the named temporary task.
Returns the number of checks or throws if any check or cleanup fails.
#>
function Test-TaskRegistration {
    param([string]$Install, [string]$Exe, [string]$TaskName)

    & $Install -DeviceType laptop -ExePath $Exe -TaskName $TaskName | Out-Null
    try {
        Assert-RegisteredRepetition -TaskName $TaskName
        Assert-TaskEnabled -TaskName $TaskName -Enabled $true
        Disable-ScheduledTask -TaskName $TaskName | Out-Null
        Assert-TaskEnabled -TaskName $TaskName -Enabled $false
        Enable-ScheduledTask -TaskName $TaskName | Out-Null
        Assert-TaskEnabled -TaskName $TaskName -Enabled $true
    }
    finally {
        Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
    }
    return 6
}

# The user scripts must also run in Windows PowerShell 5.1, which misreads non-ASCII text in files without a BOM.
foreach ($file in @($install, $uninstall)) {
    if ([IO.File]::ReadAllBytes($file) | Where-Object { $_ -gt 127 }) { throw "Non-ASCII character in $file" }
}

$directory = Join-Path ([IO.Path]::GetTempPath()) ('EcoWattSense-AutostartTest-' + [guid]::NewGuid().ToString('N'))
$checks = 0
try {
    # A folder with a space, to check the quotes around the exe path.
    $appFolder = Join-Path $directory 'App Folder'
    $null = New-Item -ItemType Directory -Path $appFolder
    $exe = Join-Path $appFolder 'EcoWattSense.Client.Gui.exe'
    Set-Content -LiteralPath $exe -Value 'dummy exe for the test'

    # A copy of the release folder: both scripts next to a dummy GUI exe.
    $releaseFolder = Join-Path $directory 'Release Folder'
    $null = New-Item -ItemType Directory -Path $releaseFolder
    $releaseExe = Join-Path $releaseFolder 'EcoWattSense.Client.Gui.exe'
    Set-Content -LiteralPath $releaseExe -Value 'dummy exe for the test'
    Copy-Item -LiteralPath $install, $uninstall -Destination $releaseFolder
    $releaseInstall = Join-Path $releaseFolder 'Install-Autostart.ps1'
    $releaseUninstall = Join-Path $releaseFolder 'Uninstall-Autostart.ps1'

    # Prints the task definition as JSON, so both engines can be checked the same way.
    $summary = Join-Path $directory 'summary.ps1'
    Set-Content -LiteralPath $summary -Encoding ascii -Value @'
param([string]$Install, [string]$Exe, [string]$DeviceType)
$ErrorActionPreference = 'Stop'
# Without -Exe, the script must find the exe next to itself (the default of -ExePath).
$arguments = @{ DeviceType = $DeviceType; DefinitionOnly = $true }
if ($Exe) { $arguments.ExePath = $Exe }
$task = & $Install @arguments
$s = $task.Settings
[ordered]@{
    TriggerCount = @($task.Triggers).Count
    TriggerClass = $task.Triggers[0].CimClass.CimClassName
    TriggerUser = $task.Triggers[0].UserId
    TriggerDelay = $task.Triggers[0].Delay
    TriggerEnabled = $task.Triggers[0].Enabled
    RepetitionInterval = $task.Triggers[0].Repetition.Interval
    RepetitionHasNoDuration = [string]::IsNullOrEmpty($task.Triggers[0].Repetition.Duration)
    ActionCount = @($task.Actions).Count
    Execute = $task.Actions[0].Execute
    Arguments = $task.Actions[0].Arguments
    WorkingDirectory = $task.Actions[0].WorkingDirectory
    PrincipalUser = $task.Principal.UserId
    LogonType = [string]$task.Principal.LogonType
    RunLevel = [string]$task.Principal.RunLevel
    DisallowStartIfOnBatteries = $s.DisallowStartIfOnBatteries
    StopIfGoingOnBatteries = $s.StopIfGoingOnBatteries
    ExecutionTimeLimit = $s.ExecutionTimeLimit
    RestartCount = $s.RestartCount
    RestartInterval = $s.RestartInterval
    MultipleInstances = [string]$s.MultipleInstances
    StartWhenAvailable = $s.StartWhenAvailable
} | ConvertTo-Json
'@

    foreach ($engine in $engines) {
        $name = Split-Path -Leaf $engine

        # 'Laptop' with a capital letter: the task must still pass the lowercase value.
        $json = & $engine -NoProfile -ExecutionPolicy Bypass -File $summary -Install $install -Exe $exe -DeviceType Laptop
        if ($LASTEXITCODE -ne 0) { throw "${name}: building the task failed: $json" }
        $t = ($json -join "`n") | ConvertFrom-Json
        $expected = [ordered]@{
            TriggerCount = 1; TriggerClass = 'MSFT_TaskLogonTrigger'; TriggerUser = $user
            TriggerDelay = 'PT1M'; TriggerEnabled = $true; ActionCount = 1
            RepetitionInterval = 'PT5M'; RepetitionHasNoDuration = $true
            Execute = '"' + $exe + '"'; Arguments = '--start-hidden --Reporting:DeviceType=laptop'; WorkingDirectory = $appFolder
            PrincipalUser = $user; LogonType = 'Interactive'; RunLevel = 'Limited'
            DisallowStartIfOnBatteries = $false; StopIfGoingOnBatteries = $false
            ExecutionTimeLimit = 'PT0S'; RestartCount = 3; RestartInterval = 'PT1M'
            MultipleInstances = 'IgnoreNew'; StartWhenAvailable = $true
        }
        foreach ($key in $expected.Keys) {
            if ($t.$key -ne $expected[$key]) { throw "${name}: $key is '$($t.$key)', expected '$($expected[$key])'" }
            $checks++
        }
        if ($t.Arguments -match 'ClientKey|X-EWS-Key') { throw "${name}: the client key must never be a task argument" }
        $checks++

        # Without -ExePath, the script uses the exe in its own folder, as in the release ZIP.
        # The script is started with -File, as in the README.
        $json = & $engine -NoProfile -ExecutionPolicy Bypass -File $summary -Install $releaseInstall -DeviceType desktop 2>&1
        if ($LASTEXITCODE -ne 0) { throw "${name}: install without -ExePath failed: $json" }
        $d = ($json -join "`n") | ConvertFrom-Json
        if ($d.Execute -ne '"' + $releaseExe + '"') { throw "${name}: default exe is '$($d.Execute)', expected '$releaseExe'" }
        if ($d.WorkingDirectory -ne $releaseFolder) { throw "${name}: default working directory is '$($d.WorkingDirectory)'" }
        if ($d.Arguments -ne '--start-hidden --Reporting:DeviceType=desktop') { throw "${name}: default arguments are '$($d.Arguments)'" }
        $checks += 3

        # The same, with the script run directly by -File (no wrapper script in between).
        $out = & $engine -NoProfile -ExecutionPolicy Bypass -File $releaseInstall -DeviceType laptop -DefinitionOnly 2>&1
        if ($LASTEXITCODE -ne 0) { throw "${name}: install -File without -ExePath failed: $out" }
        $checks++

        # Uninstall started by -File from the release folder (no path parameters to resolve).
        $out = & $engine -NoProfile -ExecutionPolicy Bypass -File $releaseUninstall -TaskName $taskName 2>&1
        if ($LASTEXITCODE -ne 0 -or ($out -join ' ') -notmatch 'not installed') { throw "${name}: uninstall from the release folder failed: $out" }
        $checks++

        # A missing exe stops the script with a clear message.
        $out = & $engine -NoProfile -ExecutionPolicy Bypass -File $install -DeviceType laptop -ExePath (Join-Path $directory 'missing.exe') -DefinitionOnly 2>&1
        if ($LASTEXITCODE -eq 0 -or ($out -join ' ') -notmatch 'Client exe not found') { throw "${name}: missing exe was not reported: $out" }
        $checks++

        # Only laptop and desktop are accepted.
        $out = & $engine -NoProfile -ExecutionPolicy Bypass -File $install -DeviceType tablet -ExePath $exe -DefinitionOnly 2>&1
        if ($LASTEXITCODE -eq 0) { throw "${name}: device type 'tablet' was accepted" }
        $checks++

        # Removing a task that does not exist is not an error.
        $out = & $engine -NoProfile -ExecutionPolicy Bypass -File $uninstall -TaskName $taskName 2>&1
        if ($LASTEXITCODE -ne 0 -or ($out -join ' ') -notmatch 'not installed') { throw "${name}: uninstall of a missing task failed: $out" }
        $checks++
    }

    # A folder the user cannot write to is refused. Deny "create files" on a test folder for the current user.
    $locked = Join-Path $directory 'Locked'
    $null = New-Item -ItemType Directory -Path $locked
    $lockedExe = Join-Path $locked 'EcoWattSense.Client.Gui.exe'
    Set-Content -LiteralPath $lockedExe -Value 'dummy exe for the test'
    $sid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    $null = icacls $locked /deny "*${sid}:(WD)"
    if ($LASTEXITCODE -ne 0) { throw 'Could not lock the test folder' }
    try {
        $out = & $engines[0] -NoProfile -ExecutionPolicy Bypass -File $install -DeviceType laptop -ExePath $lockedExe -DefinitionOnly 2>&1
        if ($LASTEXITCODE -eq 0 -or ($out -join ' ') -notmatch 'cannot write to the folder') { throw "Not writable folder was not reported: $out" }
        $checks++
    }
    finally { $null = icacls $locked /remove:d "*$sid" }

    $checks += Test-TaskRegistration -Install $install -Exe $exe -TaskName $taskName

    # Nothing may be left behind in Task Scheduler.
    if (Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue) { throw 'A test task was left behind' }
    $checks++

    Write-Output "Passed $checks autostart checks ($($engines.Count) PowerShell version(s))."
} finally {
    $resolved = [IO.Path]::GetFullPath($directory)
    $parent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar)
    if ([IO.Path]::GetDirectoryName($resolved) -ne $parent -or ![IO.Path]::GetFileName($resolved).StartsWith('EcoWattSense-AutostartTest-')) { throw 'Unsafe cleanup path' }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
