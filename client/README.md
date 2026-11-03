# Monitoring client

The Windows client monitors user inactivity, CPU usage, screen lock and battery/mains power. It classifies activity as Active, Idle or Unused and generates reports.

Server communication is disabled by default. The default reporting interval is 300 seconds.

## Layout

```text
client/
  windows/
    EcoWattSense.Client/        Windows monitoring client
    EcoWattSense.Client.Gui/    WPF window for the same client
    scripts/                    Start automatically at login (install and uninstall)
    EcoWattSense.Client.Tray/   Future tray app, if needed
  linux/                       Future Linux client
  macos/                       Future macOS client
  spike/                       Small test scripts
```

The tray app, Linux client and macOS client are not implemented yet.

## Run on Windows

Requires the .NET 10 SDK. From the repository root:

```powershell
dotnet run --project ./client/windows/EcoWattSense.Client -- --Reporting:DeviceType=laptop
```

Use `desktop` instead of `laptop` when appropriate.

## Start automatically at login

The client can start by itself every time you log in to Windows. It then runs all day with
no window, on battery too. This uses a Task Scheduler task for your own account.
You do not need admin rights and no password is stored.

The release ZIP has a special exe for this, `EcoWattSense.Client.Background.exe`. It is the
same client as `EcoWattSense.Client.exe`, but it opens no window.

### Install

1. Unzip the release into `%LOCALAPPDATA%\Programs\EcoWattSense`. The client saves files next
   to the exe, so use a folder you can write to (not Program Files).
2. Open PowerShell in that folder. Windows blocks scripts that come from a downloaded ZIP, so unblock them once:

   ```powershell
   Get-ChildItem *.ps1 | Unblock-File
   ```

3. Install the task. Use `desktop` instead of `laptop` when appropriate:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\Install-Autostart.ps1 -DeviceType laptop
   ```

The task is called `EcoWattSense Client`. It starts the client one minute after you log in,
so that Wi-Fi is up first. Run the same command again to change the device type or after you
move the folder. It updates the task.

Do not start `EcoWattSense.Client.Background.exe` by double-click. Without the task it gets no
device type and has no window to ask for one. So it stops at once and shows nothing.
The error is only in Event Viewer. If a device type was saved before (for example by the GUI),
it does start, but you cannot see it, and it runs next to the task as a second copy.

### Server key

The task never contains the client key, because anyone can read task settings in Task Scheduler.
If you send reports to the server, save the settings as user environment variables instead:

```powershell
[Environment]::SetEnvironmentVariable('Server__Enabled', 'true', 'User')
[Environment]::SetEnvironmentVariable('Server__BaseUrl', 'https://<server address>', 'User')
[Environment]::SetEnvironmentVariable('Server__ClientKey', '<your client key>', 'User')
```

Then sign out and sign in again, so the task sees them. Never put the key in a file in the repository.

### Check that it works

After you log in, wait a minute, then:

```powershell
Get-ScheduledTask -TaskName 'EcoWattSense Client' | Select-Object State        # Running
Get-Process EcoWattSense.Client.Background                                    # the client process
Get-ChildItem "$env:LOCALAPPDATA\EcoWattSense\Reports" | Sort-Object LastWriteTime | Select-Object -Last 3
```

A new report file appears after each reporting interval (5 minutes by default). The activity
history is in the `Reports` folder next to the exe.

The client has no window, so errors go to Event Viewer: Windows Logs, Application, sources
".NET Runtime" and "Application Error".

To start it now without signing out: `Start-ScheduledTask -TaskName 'EcoWattSense Client'`.
To stop it until the next login: `Stop-ScheduledTask -TaskName 'EcoWattSense Client'`
(or end `EcoWattSense.Client.Background.exe` in Task Manager).

Do not open the GUI while the task is running. Then two copies would monitor the same computer
and the server would get two sets of reports.

### Uninstall

```powershell
powershell -ExecutionPolicy Bypass -File .\Uninstall-Autostart.ps1
```

This stops the client and removes the task. Your reports and settings stay. Then you can
delete the folder.

See the project's Reporting and Communication READMEs for configuration and test instructions.