# Monitoring client

The Windows client monitors user inactivity, CPU usage, screen lock and battery/mains power. It classifies activity as Active, Idle or Unused and generates reports.

Server communication is disabled by default. The default reporting interval is 300 seconds.

## Layout

```text
client/
  windows/
    EcoWattSense.Client/        Windows monitoring client
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

See the project's Reporting and Communication READMEs for configuration and test instructions.