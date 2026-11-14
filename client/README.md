# Monitoring client

The Windows client monitors user inactivity, CPU usage, screen lock and battery/mains power. It classifies activity as Active, Idle or Unused and generates reports.

Server communication is disabled by default. The default reporting interval is 300 seconds.

## Layout

```text
client/
  windows/
    EcoWattSense.Client/        Shared monitoring code and development console client
    EcoWattSense.Client.Gui/    Windows tray app and monitoring window
    scripts/                    Start automatically at login (install and uninstall)
  linux/                       Future Linux client
  macos/                       Future macOS client
  spike/                       Small test scripts
```

## Run on Windows

Extract the release ZIP and open `EcoWattSense.Client.Gui.exe` to see live readings.
Closing the window with **X** keeps monitoring in the tray. Choose **Open** from the
tray menu or launch the EXE again to restore the window. Choose **Exit** to stop monitoring.

To run the development console client from the repository root, use the .NET 10 SDK:

```powershell
dotnet run --project ./client/windows/EcoWattSense.Client -- --Reporting:DeviceType=laptop
```

Use `desktop` instead of `laptop` when appropriate.

## Start automatically at login

The client can start by itself every time you log in to Windows. It then runs all day with
no window, on battery too. This uses a Task Scheduler task for your own account.
You do not need admin rights and no password is stored.

The ZIP contains the GUI EXE, `appsettings.json`, and the install and uninstall scripts.
The task runs the same GUI with `--start-hidden` and the selected device type.
Monitoring starts before you first open the window.

### Install

1. Unzip the release into `%LOCALAPPDATA%\Programs\EcoWattSense`. The client saves files next
   to the exe, so use a folder you can write to (not Program Files).
   The ZIP has one folder inside, `EcoWattSense.Client-<version>-win-x64`, so the files end up in
   `%LOCALAPPDATA%\Programs\EcoWattSense\EcoWattSense.Client-<version>-win-x64`.
2. Open PowerShell in the folder that contains `Install-Autostart.ps1` (the folder named
   `EcoWattSense.Client-<version>-win-x64`). Windows blocks files that come from a downloaded ZIP,
   so unblock all of them once:

   ```powershell
   Get-ChildItem -Recurse | Unblock-File
   ```

3. Install the task. Use `desktop` instead of `laptop` when appropriate:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\Install-Autostart.ps1 -DeviceType laptop
   ```

The task is called `EcoWattSense Client`. It starts the client one minute after you log in,
so that Wi-Fi is up first. Run the same command again to change the device type or after you
move the folder. It updates the task.

### Upgrade from the background EXE

1. In the old release folder, run `powershell -ExecutionPolicy Bypass -File .\Uninstall-Autostart.ps1`.
   This stops the scheduled `EcoWattSense.Client.Background.exe` and removes its task.
   If you started that EXE separately, stop that copy too so it releases the monitoring lock.
2. Extract the new release into its own folder. Keep the old folder if you need its
   activity history, and carry over any custom settings from its `appsettings.json`.
3. Run the install command above from the **new** folder with your device type.
   The task now points at the new GUI. It reuses the machine's existing device identity.

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
Get-Process EcoWattSense.Client.Gui                                           # the tray client
Get-ChildItem "$env:LOCALAPPDATA\EcoWattSense\Reports" | Sort-Object LastWriteTime | Select-Object -Last 3
```

A new report file appears after each reporting interval (5 minutes by default). The activity
history is in the `Reports` folder next to the exe.

For startup diagnostics, open Event Viewer, Windows Logs, Application,
source ".NET Runtime". At each start the client writes a few lines (file paths, settings, device id).
After that it writes only warnings and errors, not every reading, so the log does not fill up.
A crash is also shown with the source "Application Error".

### Recovery, pause and resume

After the logon trigger fires, Task Scheduler tries to start the client every five minutes
for the rest of the session. It skips a repeat while the scheduled task is running. If
the client crashes or exits, the next repeat can start it again in the tray. After installing
or updating the task, sign out and back in to activate the logon trigger.

To pause automatic launches, disable the task:

```powershell
Disable-ScheduledTask -TaskName 'EcoWattSense Client'
```

The client may still be running. Choose **Exit** from its tray menu to stop monitoring.
If the tray is unavailable and Task Scheduler started the client, use
`Stop-ScheduledTask -TaskName 'EcoWattSense Client'` after disabling the task.
Stopping the client by itself leaves automatic launches enabled.

To resume automatic launches, enable the task and sign out and back in to activate
the logon trigger again:

```powershell
Enable-ScheduledTask -TaskName 'EcoWattSense Client'
```

To start the client immediately while you are still signed in, use
`Start-ScheduledTask -TaskName 'EcoWattSense Client'`.

### Only one copy runs

Only one copy monitors a computer at a time. A manual GUI launch opens the running
GUI in your Windows session. If the development console, an old background EXE,
or a GUI in another session owns the lock, the manual launch explains why it cannot
open the window and exits. A hidden launch that finds the lock held exits quietly.
The development console still exits with code 2 when the lock is held.

The lock is the file `instance.lock` in `%ProgramData%\EcoWattSense\Client`, next to the device id.
Windows frees it when the copy ends, also after a crash. You do not need to delete it.

Known limitation: the lock is for the whole computer, like the device id. If two Windows users
are logged in at the same time (fast user switching), only the copy of the user who started it
first monitors. The second user's session does not produce readings.

### Uninstall

```powershell
powershell -ExecutionPolicy Bypass -File .\Uninstall-Autostart.ps1
```

This stops the client and removes the task. Your reports and settings stay. Then you can
delete the folder.

See the project's Reporting and Communication READMEs for configuration and test instructions.
