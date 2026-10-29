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
   command. Choose Laptop or Desktop, close, and relaunch with the same argument;
   the saved choice should be reused. Cancelling the dialog should show a startup
   failure and never show running.
2. Verify the five cards and last-reading time update about every five seconds.
   The first CPU sample may be unavailable while it warms up. Leave the machine
   idle, resume input, lock/unlock, and change power source if supported.
3. Run with `-- --StateClassification:IdleThresholdSeconds=0`; verify a visible
   error and no running message. Remove the override for normal startup.
4. Close during startup and while monitoring. Verify the GUI process exits and no
   child monitoring process remains. Reporting/network shutdown is awaited.
5. Let a normal run reach a full reporting interval (default 300 seconds). Inspect
   the existing reports under `%LOCALAPPDATA%/EcoWattSense/Reports` and existing
   server logs when configured. The GUI does not claim reports were saved or sent.

The integration check uses isolated temporary identity/history files, two actual
Windows readings, and the normal 300-second report interval; it stops before any
report is due. It checks singleton registration, saved/configured type reuse,
observer-failure isolation, sampling cadence, classification, and worker shutdown.
