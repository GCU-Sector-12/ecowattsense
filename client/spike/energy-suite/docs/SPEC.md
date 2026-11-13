# EcoWattSense energy suite for Windows 11: specification (review version)

This file is the contract between the five programs of the suite. Every agent that writes code for the suite reads it first and builds it one to one. A part that cannot be built as written is a question to the main session, not a substitute.

Status: spike. The work lives on the branch `EWS-energy-suite` and is not merged into `dev`.

## 1. Programs

| Program | Language | Runs as | Job |
|---|---|---|---|
| `ews-scan.exe` | C++ | the caller | Tests every energy data source of this computer. Writes `capabilities.json` and a log. |
| `ews-service.exe` | C++ | Windows service, from system start | Measures once a minute, builds a report every interval, sends it, keeps the 24 h history. |
| `EnergyCalibrator.exe` | C# WPF | the logged-on user | Runs the battery test and writes `model.json`. Can be run again to refresh it. |
| `EcoWattSense.Client.Gui.exe` | C# WPF | the logged-on user | Tray program: shows the service data and the last 24 h, passes the idle time to the service. |
| `Setup.exe` | C# WPF | administrator | Scans, logs, installs, configures, offers the calibration, uninstalls. |

`ews-scan.exe` and `ews-service.exe` share one static library, `ews-core` (readers, scan, energy calculation, file formats).

Target: Windows 11, x64. Windows 10 and ARM64 are out of scope.

## 2. Folders

### 2.1 Repository

```
client/spike/energy-suite/
  docs/SPEC.md            this file
  native/                 C++ solution: ews-core, ews-scan, ews-service, ews-core-tests
  setup/                  Setup.exe (C# WPF)
  scripts/                Build-Native.ps1, Test-Native.ps1, Build-Package.ps1
client/spike/energy-calibrator/      the calibrator (exists)
client/windows/EcoWattSense.Client.Gui/   the tray client (exists, gets a viewer mode)
```

### 2.2 Installed files

Programs: `C:\Program Files\EcoWattSense\`.

Data: `C:\ProgramData\EcoWattSense\` (below: `<data>`).

| Folder | Holds | Service account | Users | Administrators, SYSTEM |
|---|---|---|---|---|
| `<data>\Client\` | `device-id.txt`, `device-type.txt`, `instance.lock` (exist today) | modify | as today | full |
| `<data>\scan\` | `capabilities.json`, `capabilities.service.json`, `scan.log` | modify | read | full |
| `<data>\calibration\` | `model.json`, `model.previous.json` | read | modify | full |
| `<data>\history\` | one CSV file per day | modify | read | full |
| `<data>\status\` | `status.json` | modify | read | full |
| `<data>\queue\` | `reports.jsonl` | modify | none | full |
| `<data>\config\` | `service.ini`, `key.bin` | read | none | full |
| `<data>\logs\` | `install.log`, `service.log` | modify | read | full |

Users can write only `calibration\`. The service treats that file as untrusted (section 8).

## 3. Scanner

### 3.1 Command line

`ews-scan.exe [--out <folder>] [--context user|admin|service] [--quiet]`

- Without `--out` the files go to the current folder. Setup passes `<data>\scan`.
- The scanner needs no administrator rights and installs nothing.
- Exit codes: 0 scan written, 2 output folder not writable, 3 bad arguments.

### 3.2 The test of one source

Each source goes through the same four steps. The result of each step is stored.

1. `present`: the device, counter or library exists.
2. `readable`: it opens and one read succeeds. A failure stores the Windows error code.
3. `live`: two reads, 3 seconds apart, with a 1 second processor load in between. The source is live when the value changed. A source whose value cannot change in 3 seconds (battery charge) stores `live: null`.
4. `sample`: one value in the unit of the source, for the log.

A failed step is "unknown", never "does not exist". The scan never stops on one failed source.

### 3.3 Sources

| Id | What | How | Used by the service |
|---|---|---|---|
| `emi` | Every Energy Meter Interface device and every channel of it | SetupAPI with `GUID_DEVICE_ENERGY_METER`, then the four EMI IOCTLs, handle opened with `GENERIC_READ` only | yes |
| `battery` | Every battery: unit (mWh or relative), full charge, whether the rate is known, size of one charge step | Battery class IOCTLs | yes |
| `acpi_power_meter` | Whole-computer power meter | Performance counter set "Power Meter" through PDH, English names | yes, when live |
| `display` | Brightness and display on or off | `IOCTL_VIDEO_QUERY_DISPLAY_BRIGHTNESS` on `\\.\LCD`; display state from power notifications | yes |
| `gpu` | List of graphics adapters (DXGI); power from NVML when `nvml.dll` loads | DXGI, dynamic load of NVML | no, logged only (section 12, point 1) |
| `intel_dtt` | Intel DTT participants | presence of the `esifsvc` service | no, logged only |
| `machine` | Maker, model, chassis type (SMBIOS), processor vendor and name, logical processors, Windows build | `GetSystemFirmwareTable`, CPUID | yes (device type) |

EMI channel kinds, set from the channel name:

| Kind | Names | Counted |
|---|---|---|
| `system` | `PSYS`, `SYSTEM`, `PLATFORM`, `TOTAL` as a whole word in the name | alone, as the whole computer |
| `cpu_package` | `RAPL_Package<N>_PKG`, `CPU` | yes |
| `dram` | `RAPL_Package<N>_DRAM` | yes |
| `cpu_cores` | `RAPL_Package<N>_PP0`, `CPU_*` | no, part of the package |
| `igpu` | `RAPL_Package<N>_PP1` | no, part of the package |
| `other` | every other name | no, logged |

### 3.4 Measurement level

| Level | Rule | Meaning |
|---|---|---|
| `L1` | a live `system` channel, or a live ACPI power meter | The whole computer is measured. |
| `L2` | a live `cpu_package` channel | Processor and memory are measured. The rest comes from the calibration model. |
| `L3` | no live energy channel, battery reports mWh | Everything on mains comes from the calibration model. |
| `L4` | none of the above | The service sends no `energy_uj`. The server estimates from its table. |

Calibration: `not_needed` for L1, `needed` for L2 and L3 when a battery that reports mWh is present, `not_possible` when there is no such battery. L2 or L3 without a valid model works as L4 until a model exists.

`device_type`: `laptop` when the chassis type is a portable one or a battery is present, else `desktop`.

### 3.5 `capabilities.json`

```json
{
  "schema": 1,
  "scanned_utc": "2026-11-14T10:00:00Z",
  "context": "user",
  "machine": { "maker": "HP", "model": "HP 250 G7 Notebook PC", "chassis": "laptop",
               "cpu_vendor": "GenuineIntel", "cpu_name": "...", "logical_processors": 8,
               "windows_build": 26300 },
  "sources": [
    { "id": "emi", "present": true, "readable": true, "live": true, "error": 0,
      "channels": [ { "device": 0, "index": 0, "name": "RAPL_Package0_PKG",
                      "kind": "cpu_package", "live": true, "sample_w": 4.1 } ] },
    { "id": "battery", "present": true, "readable": true, "live": null, "error": 0,
      "count": 1, "unit": "mWh", "rate_known": false, "full_mwh": 36092 }
  ],
  "level": "L2",
  "calibration": "needed",
  "device_type": "laptop"
}
```

The file is masked: no serial number, no user name, no computer name, no device path. It can be collected from other laptops as it is.

`scan.log` holds one line per step of each source: source id, step, result word, error code, sample value.

## 4. Energy of one interval

All energy values are whole microjoules. 1 mWh = 3 600 000 µJ.

### 4.1 On battery

- `energy_uj` = 0 (rule of the server contract).
- `battery_energy_uj` = charge at the end minus charge at the start, in µJ. It is negative.
- `device_energy_uj` = minus `battery_energy_uj`.
- A battery with a relative unit or an unknown charge: both battery fields are left out.

### 4.2 On mains

Device power of one minute:

- L1: the `system` channel, or the ACPI power meter.
- L2: sum of the model's `measured_parts` from EMI + `rest_w` + `rest_w_per_cpu_pct` × processor percent + screen power.
- L3: `base_w` + `cpu_w_per_pct` × processor percent + screen power.
- Screen power: 0 when the display is off; else `screen_w_at_50` + (brightness − 50) × `screen_w_per_pct`, not below 0. An unknown brightness counts as 50.

Then:

- `device_energy_uj` = sum of the minutes of the interval.
- `battery_energy_uj` = charge change in µJ (positive while charging), when the battery reports mWh.
- `energy_uj` = `device_energy_uj` + the positive part of `battery_energy_uj`.
- The loss of the power adapter is not measured and not added.

L2 is used only when every part named in `measured_parts` is live. If one is missing, the interval is calculated as L3.

### 4.3 Unknown values

- A failed read, a counter that went backwards, or the first interval after a start: that value is unknown for the interval.
- An unknown value needed for a field: the field is left out of the report. It is never sent as 0.
- L4, or L2 and L3 without a valid model: `energy_uj` is left out on mains. On battery section 4.1 applies in full.

### 4.4 Interval borders

- A change of the power source ends the interval at once and starts a new one. Each report then has one true `on_battery` value.
- Suspend ends the interval. Resume starts a new one. No report covers the time asleep.
- An interval shorter than 1 second is dropped.

## 5. Service

### 5.1 Registration

- Name `EcoWattSenseService`, display name "EcoWattSense Energy Service", automatic (delayed) start.
- Account `NT AUTHORITY\LocalService`. If the scan in the service context shows a source denied that the user context can read, the account becomes LocalSystem; the main session reports that.
- Recovery: restart after 60 seconds, three times.

### 5.2 Command line

- no argument: run under the service control manager.
- `--console [--data-dir <folder>]`: run the same loop in a console, for tests.
- `--configure`: read `url=<...>` and `key=<...>` lines from standard input and write `service.ini` and `key.bin`. The key is never a command-line argument.

### 5.3 Loop

- At start: report "running" to the service control manager first, then take `<data>\Client\instance.lock` (so the old client mode cannot report at the same time), read the device identity, run the scan with context `service` into `capabilities.service.json`, read the model.
- One waitable timer, 60 seconds. No other timer. The timer does not wake the computer.
- Each tick: read the cumulative counters (EMI channels, battery charge, `GetSystemTimes`), calculate the minute, append one history row.
- Every `interval_seconds` (default 300): build the report, append it to the queue, try to send, write `status.json`, check whether `model.json` changed (file time) and reload it.
- Events, no polling: power source change, suspend and resume, display on or off, session logon, logoff, lock and unlock (service control handler).
- Stop: finish the current write, release the lock, exit within 5 seconds.

### 5.4 Report fields

| Field | Source |
|---|---|
| `device_id` | `<data>\Client\device-id.txt` (one GUID in "D" format and a line feed). A missing file is created the same way the client does it. |
| `timestamp` | end of the interval, UTC |
| `interval_seconds` | real length of the interval |
| `idle_seconds`, `state` | section 6 |
| `cpu_percent` | average over the interval from `GetSystemTimes` |
| `screen_locked` | session events; true when nobody is logged on |
| `on_battery` | power status |
| `os` | `windows` |
| `device_type` | `service.ini` |
| `energy_uj`, `battery_energy_uj`, `device_energy_uj` | section 4 |

The JSON is written by hand with exactly these fields. The server refuses unknown fields.

### 5.5 Queue and sending

- `<data>\queue\reports.jsonl`: one report per line, oldest first, at most 2016 lines (7 days). When full, the oldest line is dropped and counted in the log.
- Send: `POST <server_url>/api/reports`, header `X-EWS-Key`, body a list of up to 100 reports, WinHTTP, 10 second timeout, no redirects. HTTPS only; HTTP is allowed only for loopback.
- 200 or 201: sent lines leave the queue. An item marked `rejected` leaves the queue and is counted.
- 401, 429, 5xx, network error: the queue stays. The next try waits 1, 2, 4, ... up to 60 intervals.
- Sending happens only at a report tick.
- `server_enabled=0`: reports are queued and not sent.

### 5.6 `service.ini`

```
server_enabled=1
server_url=https://example.org
interval_seconds=300
idle_threshold_seconds=600
unused_threshold_seconds=3600
device_type=laptop
```

`interval_seconds` is a multiple of 60 between 60 and 3600. A bad value falls back to the default and is counted in the log. Server-side settings (`GET /api/settings`) are not read by the service.

`key.bin` is the server key protected with DPAPI (machine scope). The folder rights of `config\` are the real protection.

### 5.7 `status.json`

```json
{ "schema": 1, "service_version": "0.1.0", "level": "L2", "model_generated_utc": "2026-11-12T17:08:31Z",
  "last_report_utc": "2026-11-14T10:05:00Z", "last_send_utc": "2026-11-14T10:05:01Z",
  "last_send_result": "ok", "queued": 0 }
```

`last_send_result` is one of: `ok`, `disabled`, `unauthorised`, `rejected`, `server_error`, `network`, `never`. Written to a temporary file and renamed.

### 5.8 Log

`service.log` holds counts and fixed words only: no URL, no key, no file content, no server text. One line per start, stop, level change, send result and counted error. The file is capped at 1 MB; then it is renamed to `service.previous.log`.

## 6. Session pipe (client to service)

- Name `\\.\pipe\EcoWattSense.Service.Session`. The service is the server. Inbound only, message mode, up to 4 instances.
- Access: interactive users may write.
- One message, ASCII, at most 128 bytes: `EWS1 idle=<seconds> bright=<0-100 or ->\n`
- The client sends one message every 60 seconds and one on each unlock.
- The service accepts a message only when the client's session is the active console session. A malformed or out-of-range message is dropped and counted.
- `idle_seconds` of a report = idle value of the last message + seconds since that message, when the message is younger than 120 seconds. Without a fresh message: seconds since the last fresh message, or since the service start.
- `state`: 2 below `idle_threshold_seconds`, 1 below `unused_threshold_seconds`, else 0. Nobody logged on gives 0.
- Brightness: the service reads it itself when the scan in the service context says `display` is readable. Else it uses the value of the pipe.

## 7. History

- `<data>\history\<yyyy-MM-dd>.csv`, UTC date. Files older than 2 days are deleted at the change of day.
- First line: `v1,ts_utc,seconds,on_battery,state,idle_s,cpu_pct,device_w,measured_w,battery_w,level`
- One row per minute, appended, opened with shared read. An unknown value is an empty field.
- `device_w`: power of the whole computer (measured or from the model). `measured_w`: the part read from hardware. `battery_w`: battery flow, negative when discharging.
- Readers skip a row they cannot parse and count it.

## 8. Calibration model

`<data>\calibration\model.json`, written by the calibrator. The file of today's calibrator (no `schema` field) is version 1 and is not read by the service.

```json
{
  "schema": 2,
  "generated_utc": "2026-11-12T17:08:31Z",
  "machine": { "maker": "HP", "model": "HP 250 G7 Notebook PC", "cpu": "...", "logical_processors": 8, "windows": "10.0.26300.0" },
  "measured_parts": ["cpu_package", "dram"],
  "model": {
    "base_w": 3.48,
    "rest_w": 2.24,
    "rest_w_per_cpu_pct": 0.009,
    "cpu_w_per_pct": 0.075,
    "screen_w_at_50": 1.40,
    "screen_w_per_pct": 0.0104,
    "graphics_w": 2.44
  },
  "phase_power_w": { },
  "cpu_share_by_phase": { },
  "warnings": []
}
```

- `measured_parts`: the EMI kinds the calibrator could read during the test. Empty when it read none (then the model serves L3 only).
- `rest_w`: battery power minus the measured parts in the "screen off" phase.
- `rest_w_per_cpu_pct`: growth of that rest between "screen off" and "CPU 100%", per processor percent. 0 when it cannot be calculated.
- `base_w`, `cpu_w_per_pct`, `screen_w_at_50`, `screen_w_per_pct`, `graphics_w`: as today.
- Before writing, the calibrator copies the current file to `model.previous.json`. It writes to a temporary file and renames it.
- Calibrator exit codes for Setup: 0 model written, 1 stopped by the user, 2 start conditions not met (no battery, charger connected, charge below 30%), 3 test aborted, 4 file not writable.

The service reads `schema`, `generated_utc`, `measured_parts` and `model` only. Limits: file at most 64 KB; watt values 0 to 500; per-percent values 0 to 5; `measured_parts` only the kinds of section 3.3. Anything outside, missing or not a number makes the whole model invalid. An invalid model is treated as no model and counted in the log.

## 9. Client (viewer mode)

- At start the client asks the service control manager for `EcoWattSenseService`.
  - Not installed: the client works as today (own sampling and reports). Nothing changes there.
  - Installed, running or stopped: viewer mode. The client does not sample, does not report and does not take `instance.lock`.
- Viewer mode sends the pipe message of section 6.
- Information panel: service state (word), measurement level with a one-line meaning, power now (last history row), energy today (kWh), power source, last report time and send result, date of the calibration model, short device id.
- 24 h view: its own control. A line of `device_w` per minute for the last 24 hours from the history files, with a band for the state. Each state is shown with a word and a colour (Active, Idle, Unused). Gaps stay gaps.
- The view refreshes on a file change event of `history\` and `status\`, and only while the window is visible. Drawing is done by WPF shapes; no frame-by-frame redraw.
- Text fields, if any, are left-aligned with the label above or to the left.

## 10. Setup

Steps, in this order. Each step writes one line to `<data>\logs\install.log` (step, result word, error code). A failed step undoes the steps before it and ends with a clear message.

1. Check: Windows 11 x64, administrator rights.
2. Run `ews-scan.exe --context admin` into `<data>\scan`. Show the result: sources found, level, whether a calibration is needed. Write the same to the log.
3. Ask for the server address and the key. Fields are left-aligned. The key field is masked; when a key is already stored and the field is empty, its placeholder is "Server key is set". An empty key field keeps the stored key.
4. Copy the programs to `C:\Program Files\EcoWattSense\`.
5. Create the data folders with the rights of section 2.2.
6. Write the configuration through `ews-service.exe --configure` and the `device_type` from the scan.
7. Register and start the service.
8. Register the client autostart for every user: a Task Scheduler task at log-on with the same trigger design as `Install-Autostart.ps1`.
9. When the scan says `needed`: offer to run the calibrator now. The user may skip it. The log records the choice and the calibrator exit code.

Uninstall (`Setup.exe /uninstall`): stop and delete the service, delete the task, delete the programs. The data folder stays unless the user ticks "Remove data".

## 11. Rules for the native code

- C++20, MSVC, x64, warning level 4, warnings are errors, static runtime. Win32 only; no third-party library.
- Flat code as in `AGENTS.md`: no control statement inside another one, no lambda with logic. `Check-Nesting.ps1` must pass.
- Every file starts with a comment (what it is for, who uses it). Every function has a `///` comment (what it does, takes, returns, what can go wrong).
- Every handle is owned by a small wrapper class that closes it.
- A reader returns a result with a status (`ok`, `unavailable`, `denied`, `error`), the Windows error code and the value. It never throws for a missing source.
- Readers sit behind an interface, so tests use fakes. Tests are a plain program, `ews-core-tests.exe`; its exit code is the number of failed checks.
- Numbers, paths and text from files, the pipe, the server and counters are untrusted: every one is range-checked before use.

## 12. Points to confirm by measurement

1. Graphics power from NVML is found by the scanner but not used in the energy sum. Using it needs the calibrator to read NVML too; that is a decision for Darek.
2. EMI reads from the LocalService account: not tested yet. The scan in the service context answers it.
3. Brightness read from session 0: not tested yet. The pipe value is the fallback.
4. EMI on AMD and on other makers: known from literature only. The scanner on real laptops answers it.
5. Size and cost of the service (file size, memory, processor time): measured at the gate of session 2 with process counters and by comparing with the service stopped.
