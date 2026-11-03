<#
.SYNOPSIS
Starts the EcoWattSense client automatically when you log in to Windows.

.DESCRIPTION
Registers one Task Scheduler task for the current user. No admin rights are needed
and no password is stored. The task starts the client with no window, one minute after
you log in. Run the script again to update the task (for example with a new device type
or a new exe path).

The client key is never put into the task, because task arguments are visible in
Task Scheduler. Set Server__ClientKey as a user environment variable instead
(see client/README.md).

Works in Windows PowerShell 5.1 and PowerShell 7.

.PARAMETER DeviceType
laptop or desktop. Required.

.PARAMETER ExePath
The client exe to start. Default: EcoWattSense.Client.Background.exe next to this script.

.PARAMETER TaskName
Name of the task. Default: EcoWattSense Client.

.PARAMETER DefinitionOnly
Build the task and return it, but do not register it. Used by the tests.

.EXAMPLE
powershell -ExecutionPolicy Bypass -File .\Install-Autostart.ps1 -DeviceType laptop
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('laptop', 'desktop')]
    [string]$DeviceType,

    [string]$ExePath = (Join-Path $PSScriptRoot 'EcoWattSense.Client.Background.exe'),

    [string]$TaskName = 'EcoWattSense Client',

    [switch]$DefinitionOnly
)

$ErrorActionPreference = 'Stop'

# ValidateSet ignores case, but the client only accepts lowercase device types.
$DeviceType = $DeviceType.ToLowerInvariant()

# Turn a relative path into a full path, based on the current PowerShell folder.
$exe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ExePath)
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    throw "Client exe not found: $exe. Unzip the release first, or give the right path with -ExePath."
}
$folder = Split-Path -Parent $exe

# The client saves its activity history in a Reports folder next to the exe,
# so the exe folder must be writable by you.
$probe = Join-Path $folder ('.ews-write-check-' + [guid]::NewGuid().ToString('N') + '.tmp')
try {
    [IO.File]::WriteAllText($probe, 'check')
    Remove-Item -LiteralPath $probe -Force
}
catch {
    throw "You cannot write to the folder $folder. The client needs to save files there. Unzip the release into $env:LOCALAPPDATA\Programs\EcoWattSense and run this script from there."
}

$user = [Security.Principal.WindowsIdentity]::GetCurrent().Name

# Task Scheduler needs quotes around a path with spaces.
$execute = $exe
if ($execute.Contains(' ')) { $execute = '"' + $execute + '"' }

# Only the device type is passed. Never pass the client key here: arguments are visible in Task Scheduler.
$action = New-ScheduledTaskAction -Execute $execute -Argument "--Reporting:DeviceType=$DeviceType" -WorkingDirectory $folder

# Start one minute after log on, so Wi-Fi is up before the client asks the server for its settings.
$trigger = New-ScheduledTaskTrigger -AtLogOn -User $user
$trigger.Delay = 'PT1M'

# Interactive: runs in your own session, only while you are logged on, with no stored password.
# The client must run in your session to see keyboard and mouse activity.
$principal = New-ScheduledTaskPrincipal -UserId $user -LogonType Interactive -RunLevel Limited

$settings = New-ScheduledTaskSettingsSet `
    -AllowStartIfOnBatteries `
    -DontStopIfGoingOnBatteries `
    -ExecutionTimeLimit ([TimeSpan]::Zero) `
    -RestartCount 3 `
    -RestartInterval (New-TimeSpan -Minutes 1) `
    -MultipleInstances IgnoreNew `
    -StartWhenAvailable

$task = New-ScheduledTask -Action $action -Trigger $trigger -Principal $principal -Settings $settings `
    -Description 'Starts the EcoWattSense client (no window) when you log in. Remove it with Uninstall-Autostart.ps1.'

if ($DefinitionOnly) {
    return $task
}

# -Force replaces the task if it already exists, so running this script again updates it.
Register-ScheduledTask -TaskName $TaskName -InputObject $task -Force | Out-Null

Write-Output "Registered task '$TaskName' for $user."
Write-Output "  Starts: $exe --Reporting:DeviceType=$DeviceType"
Write-Output "  When:   1 minute after you log in"
Write-Output "To start it now without logging out: Start-ScheduledTask -TaskName '$TaskName'"

# Warn if the server is switched on but the key is missing for the task.
$serverEnabled = [Environment]::GetEnvironmentVariable('Server__Enabled', 'User')
$clientKey = [Environment]::GetEnvironmentVariable('Server__ClientKey', 'User')
if ($serverEnabled -eq 'true' -and [string]::IsNullOrEmpty($clientKey)) {
    Write-Warning 'Server__Enabled is true but there is no user environment variable Server__ClientKey. See client/README.md.'
}
