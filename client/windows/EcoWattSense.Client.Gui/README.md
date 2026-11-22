# GUI monitoring

The GUI hosts the same singleton Worker as the console client through MonitoringHost.
Readings come from the existing five-second sampling loop; no GUI sampling timer or
child monitoring process is started. Report generation, thresholds, identity, and
HTTP request/response behavior remain in the existing client implementation.
The report status fields deliberately say "not yet connected".

The GUI reads appsettings*.json from its executable directory (copied from the client
project), plus the standard environment variables and command-line overrides. For
example, Reporting__DeviceType=laptop overrides the stored device type. The console
retains its existing working-directory configuration behavior.
Configured and saved types are validated before prompting. A new Laptop/Desktop
selection uses the same atomic persistence and concurrent-writer handling as the
console. Invalid/unreadable saved selections cause a visible startup error.

A manual launch opens the window. The login task passes `--start-hidden` to start
monitoring in the tray. The GUI removes this switch before passing configuration
arguments to the monitoring host. If the device type is missing, the selection dialog
appears even on a hidden start. A startup error displays a dialog and the copy exits
when you dismiss it.

From the repository root on Windows with the .NET 10 SDK and PowerShell 7:

```powershell
dotnet build client/windows/EcoWattSense.Client/EcoWattSense.Client.csproj
dotnet build client/windows/EcoWattSense.Client.Gui/EcoWattSense.Client.Gui.csproj
Get-ChildItem client/windows/EcoWattSense.Client/tests/Test-*.ps1 | ForEach-Object {
    & pwsh -NoProfile -File $_.FullName
    if ($LASTEXITCODE -ne 0) { throw "Failed: $($_.Name)" }
}
dotnet run --project client/windows/tests/MonitoringIntegration/MonitoringIntegration.csproj
dotnet run --project client/windows/EcoWattSense.Client.Gui/EcoWattSense.Client.Gui.csproj
```

Interactive checks:

1. With an existing configuration/selection, verify no device prompt appears.
   For a fresh selection without changing your real identity, append
   `-- --DeviceIdentity:Directory="$env:TEMP/EcoWattSense-Gui-Manual"` to the GUI run
   command. Choose Laptop or Desktop, use tray **Exit**, and relaunch with the same argument;
   the saved choice should be reused. Cancelling the dialog should show a startup
   failure and never show running.
2. Verify the five cards and last-reading time update about every five seconds.
   The first CPU sample may be unavailable while it warms up. Leave the machine
   idle, resume input, lock/unlock, and change power source if supported.
3. Run with `-- --StateClassification:IdleThresholdSeconds=0`; verify a visible
   error and no running message. Remove the override for normal startup.
4. Start with `-- --start-hidden --Reporting:DeviceType=laptop`. Before opening the
   window, check for two new readings in `Reports/activity-history.json` beside the EXE.
   Repeat the hidden launch and verify the original window stays hidden. Choose tray
   **Open**, hide the window with **X**, then launch normally to restore it.
   Choose tray **Exit** and verify a fresh launch starts monitoring again.
5. Let a normal run reach a full reporting interval (default 300 seconds). Inspect
   the existing reports under `%LOCALAPPDATA%/EcoWattSense/Reports` and existing
   server logs when configured. The GUI does not claim reports were saved or sent.

The integration check uses isolated temporary identity/history files, two actual
Windows readings, and the normal 300-second report interval; it stops before any
report is due. It checks singleton registration, saved/configured type reuse,
observer-failure isolation, sampling cadence, classification, and worker shutdown.

## Developer page (viewer mode, SPEC section 9)

The Settings page, About card, has the box "Developer mode". It is off by default and kept per user in
`%LOCALAPPDATA%\EcoWattSense\developer-mode.txt` (the text `1`, or no file). When it is on, the sidebar shows a third item,
"Developer", in viewer mode only.

The page shows the raw data that the service writes, newest first, in six tabs:

- Readings: the `tick` lines of the last 24 hours of `<data>\logs\diag-<yyyy-MM-dd>.jsonl`.
- Reports: the `report` lines. Sends and events: the `send` and `event` lines.
- Sources: `<data>\scan\capabilities.service.json` and `capabilities.json` as "name, value" rows.
- Status: `<data>\status\status.json` as "name, value" rows.
- Log: the last 300 lines of `<data>\logs\service.log`.

The page does not know the field names. It flattens each line (`calc.device_w`, `emi[0].name`, a list of words joined with a
space). The columns are the union of the names in the order of first appearance, `ts` first.

Everything read is untrusted: a line is at most 64 KB, a table at most 5000 rows (the newest) and 400 columns, a cell at most
200 characters, nesting at most 6 levels, a file at most 64 MB. A line that is not a JSON object with `t` and `ts` is skipped
and counted ("M lines could not be read"). A file that is missing or cannot be read is "unknown": the page says so and never
shows it as empty. A last line without a line end is a line the service is still writing, so it is ignored and not counted.

The data is read off the window thread when a file in `logs\`, `status\` or `scan\` changes, and only while the page is
visible. There is no timer.

"Export diagnostics..." (top right) opens a Save dialog (`EcoWattSense-diagnostics-<yyyyMMdd-HHmm>.zip`) and writes a ZIP with
`scan\capabilities*.json`, `scan\scan.log`, `scan\scan.service.log`, `history\*.csv`, `logs\service.log`,
`logs\service.previous.log`, `logs\diag-*.jsonl`, `calibration\model.json` and `status\status.json` without the fields
`server_url`, `device_name` and `server_device_name`. Never in the ZIP: `config\`, `queue\`, `Client\` and `logs\install.log`.
A file that is missing or cannot be read is left out; `export-notes.txt` in the ZIP holds the counts of files written and left
out. The line under the button says "Saved." or "The export failed.". The list of files (`DiagExportPlan`) is separate from
the ZIP writer and the dialog, so a later "send" function can use it.

The logic has no WPF in it and sits in `Developer\Logic`. Its tests are a plain console program that needs no Windows:

```powershell
dotnet run --project client/windows/tests/DeveloperPage/DeveloperPage.csproj
```

The exit code is the number of failed checks.
