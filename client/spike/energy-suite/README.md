# EcoWattSense energy suite

The suite measures how much energy a Windows computer uses and reports it to the EcoWattSense server. It is a spike: the quality is that of a prototype. The full contract is in `docs/SPEC.md`.

## The five programs

| Program | Job |
|---|---|
| `ews-scan.exe` | Tests every energy data source of the computer and writes `capabilities.json`. |
| `ews-service.exe` | Windows service. Measures every minute, builds a report each interval, sends it and keeps 24 hours of history. |
| `EnergyCalibrator.exe` | Runs a battery test and writes the calibration model. |
| `EcoWattSense.Client.Gui.exe` | Tray program. Shows the service data and the last 24 hours. |
| `Setup.exe` | Scans, installs, configures, offers the calibration and uninstalls. |

## Requirements

Windows 11, x64. Windows 10 and ARM64 are not supported. Administrator rights are needed for Setup only. The programs carry their own .NET runtime.

## Install

1. Download `EcoWattSense-<version>-win-x64.zip` and check it against the `.sha256` file.
2. Unblock the ZIP (file properties, tick Unblock) and unpack it.
3. Run `Setup.exe` as administrator and follow the steps.

The programs are not signed. Windows SmartScreen may warn: choose More info, then Run anyway.

## Measurement levels

| Level | Meaning |
|---|---|
| L1 | The whole computer is measured (a live whole-system channel or ACPI power meter). |
| L2 | Processor and memory are measured; the rest comes from the calibration model. |
| L3 | No live energy channel; everything on mains comes from the calibration model (battery reports mWh). |
| L4 | Nothing usable; the service sends no energy value and the server estimates it. |

## Calibration

Needed on L2 and L3 when the computer has a battery that reports mWh. Without a model, L2 and L3 work as L4. The test runs on battery only (charger unplugged, charge at least 30%) and takes some minutes. Leave the computer alone while it runs. Run `EnergyCalibrator.exe` again to refresh the model.

## Where data lives

- Programs: `C:\Program Files\EcoWattSense\`
- Data: `C:\ProgramData\EcoWattSense\` (scan results, calibration, history, status, queue, configuration, logs)

## Uninstall

Run `Setup.exe /uninstall` from `C:\Program Files\EcoWattSense`. It stops and removes the service, the autostart task and the programs. The data folder stays unless you tick "Remove data".

## Limits

- Charger loss is not counted: on mains the figure is the computer, not the wall socket.
- AMD processors are only noted by the scan; AMD energy counters are not read.
- This is spike quality: expect rough edges.

## Build

```
pwsh -NoProfile -File client\spike\energy-suite\scripts\Build-Package.ps1 [-Version 0.1.0] [-OutDir <folder>]
```

Needs Visual Studio 2022 with the C++ desktop workload and the .NET 10 SDK. The result is a ZIP and a `.sha256` file in `out\package`.

## Release workflow

The workflow `.github/workflows/energy-suite-release.yml` builds, signs and drafts a release. To cut a release, bump nothing and push a tag:

```
git tag v0.1.2-energy-suite
git push origin v0.1.2-energy-suite
```

The version (here 0.1.2) is taken from the tag. The workflow needs the repository secrets `ENERGY_SUITE_SIGNING_PFX` (the .pfx file, base64) and `ENERGY_SUITE_SIGNING_PASSWORD`; without them it fails at once. It makes a DRAFT pre-release with the ZIP, the web installer, both `.sha256` files and `latest.json`. A person checks the draft and publishes it, then commits `latest.json` to `client/spike/energy-suite/release/`.
