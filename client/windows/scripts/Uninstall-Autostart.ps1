<#
.SYNOPSIS
Stops the EcoWattSense client from starting at login.

.DESCRIPTION
Stops the task if it is running and removes it. If the task is already gone,
the script says so and finishes without an error. Your reports and settings are not deleted.

Works in Windows PowerShell 5.1 and PowerShell 7.

.PARAMETER TaskName
Name of the task. Default: EcoWattSense Client.

.EXAMPLE
powershell -ExecutionPolicy Bypass -File .\Uninstall-Autostart.ps1
#>
[CmdletBinding()]
param(
    [string]$TaskName = 'EcoWattSense Client'
)

$ErrorActionPreference = 'Stop'

$task = Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue
if ($null -eq $task) {
    Write-Output "Task '$TaskName' is not installed. Nothing to remove."
    return
}

# Stop the running client first, so the exe can be deleted or replaced afterwards.
Stop-ScheduledTask -TaskName $TaskName
Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
Write-Output "Removed task '$TaskName'. The client will not start at login any more."
