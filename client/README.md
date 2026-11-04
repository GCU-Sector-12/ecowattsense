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

A new version unzips into a new folder. Run `Uninstall-Autostart.ps1` from the old folder first,
then the steps above in the new folder.

Do not start `EcoWattSense.Client.Background.exe` by double-click. Without the task it gets no
device type and has no window to ask for one. So it stops at once and shows nothing.
The error is only in Event Viewer. If a device type was saved before (for example by the GUI),
it does start, but you cannot see it. If the task is already running, it does not start
a second copy (see "Only one copy runs" below).

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

The client has no window, so it writes to Event Viewer instead: Windows Logs, Application,
source ".NET Runtime". At each start it writes a few lines (file paths, settings, device id).
After that it writes only warnings and errors, not every reading, so the log does not fill up.
A crash is also shown with the source "Application Error".

If the client crashes or is ended, it does not start again by itself. It starts again at the
next login. To start it at once, use the command below.

To start it now without signing out: `Start-ScheduledTask -TaskName 'EcoWattSense Client'`.
To stop it until the next login: `Stop-ScheduledTask -TaskName 'EcoWattSense Client'`
(or end `EcoWattSense.Client.Background.exe` in Task Manager).

### Only one copy runs

Only one copy of the client monitors a computer at a time. This is the console exe, the
background exe or the GUI, whichever starts first. Another copy started later does not
monitor and sends nothing:

- The console exe and the background exe write the warning "EcoWattSense is already running
  on this computer" and stop with exit code 2. The console exe shows it in the window. The
  background exe writes it to Event Viewer (Windows Logs, Application, source ".NET Runtime").
  Task Scheduler then shows the last run result `0x2`.
- The GUI shows the message "EcoWattSense is already running on this computer" and closes.

So, while the task is running, the GUI does not show live data. To use the GUI, stop the task
first (`Stop-ScheduledTask -TaskName 'EcoWattSense Client'`). When a copy stops or crashes,
the next copy can start.

The lock is the file `instance.lock` in `%ProgramData%\EcoWattSense\Client`, next to the device id.
Windows frees it when the copy ends, also after a crash. You do not need to delete it.

Known limitation: the lock is for the whole computer, like the device id. If two Windows users
are logged in at the same time (fast user switching), only the copy of the user who started it
first monitors. The copy of the second user says it is already running, so the second user's
activity is not reported.

### Uninstall

```powershell
powershell -ExecutionPolicy Bypass -File .\Uninstall-Autostart.ps1
```

This stops the client and removes the task. Your reports and settings stay. Then you can
delete the folder.

See the project's Reporting and Communication READMEs for configuration and test instructions.