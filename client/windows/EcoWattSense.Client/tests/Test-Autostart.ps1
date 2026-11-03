# Checks the autostart scripts in client/windows/scripts without registering a task.
# Install-Autostart.ps1 is run with -DefinitionOnly, so the task is only built in memory.
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
    $exe = Join-Path $appFolder 'EcoWattSense.Client.Background.exe'
    Set-Content -LiteralPath $exe -Value 'dummy exe for the test'

    # Prints the task definition as JSON, so both engines can be checked the same way.
    $summary = Join-Path $directory 'summary.ps1'
    Set-Content -LiteralPath $summary -Encoding ascii -Value @'
param([string]$Install, [string]$Exe, [string]$DeviceType)
$ErrorActionPreference = 'Stop'
$task = & $Install -DeviceType $DeviceType -ExePath $Exe -DefinitionOnly
$s = $task.Settings
[ordered]@{
    TriggerCount = @($task.Triggers).Count
    TriggerClass = $task.Triggers[0].CimClass.CimClassName
    TriggerUser = $task.Triggers[0].UserId
    TriggerDelay = $task.Triggers[0].Delay
    TriggerEnabled = $task.Triggers[0].Enabled
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
            Execute = '"' + $exe + '"'; Arguments = '--Reporting:DeviceType=laptop'; WorkingDirectory = $appFolder
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
    $lockedExe = Join-Path $locked 'EcoWattSense.Client.Background.exe'
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
