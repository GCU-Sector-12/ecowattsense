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

| Folder | Holds | Service (`NT SERVICE\EcoWattSenseService`) | Users | Administrators, SYSTEM |
|---|---|---|---|---|
| `<data>\Client\` | `device-id.txt`, `device-type.txt`, `instance.lock` (exist today) | modify | as today | full |
| `<data>\scan\` | `capabilities.json`, `capabilities.service.json`, `scan.log` | modify | read | full |
| `<data>\calibration\` | `model.json`, `model.previous.json` | read | modify | full |
| `<data>\history\` | one CSV file per day | modify | read | full |
| `<data>\status\` | `status.json` | modify | read | full |
| `<data>\queue\` | `reports.jsonl` | modify | none | full |
| `<data>\config\` | `service.ini`, `key.bin` | read | none | full |
| `<data>\logs\` | `install.log`, `service.log` | modify | read | full |

The service rights use the service SID `NT SERVICE\EcoWattSenseService`, not `LOCAL SERVICE` (Darek's decision, 14 Nov 2026). Setup computes the SID from the service name (S-1-5-80 and a SHA-1 of the upper-case name), so the rights can be set before the service exists. `config\` holds only that SID (read), Administrators and SYSTEM; Users have no access to it.

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
3. `live`: two reads, 3 seconds apart, with a 1 second processor load in between. A source that counts energy (EMI) is live when the counter grew. A source that reports power (ACPI power meter) is live when one of the two reads is above zero. A source whose value says nothing in 3 seconds (battery, display, graphics, Intel DTT, PawnIO) stores `live: null`.
4. `sample`: one value in the unit of the source, for the log.

A failed step is "unknown", never "does not exist". The scan never stops on one failed source.

### 3.3 Sources

| Id | What | How | Used by the service |
|---|---|---|---|
| `emi` | Every Energy Meter Interface device and every channel of it | SetupAPI with `GUID_DEVICE_ENERGY_METER`, then the four EMI IOCTLs, handle opened with `GENERIC_READ` only | yes |
| `battery` | Every battery: unit (mWh or relative), full charge, whether the rate is known | Battery class IOCTLs | yes |
| `acpi_power_meter` | Whole-computer power meter | Performance counter set "Power Meter" through PDH, English names | yes, when live |
| `display` | Brightness | `IOCTL_VIDEO_QUERY_DISPLAY_BRIGHTNESS` on `\\.\LCD`. On the HP 250 G7 a standard user is denied (error 5); the scan records that. | yes, when readable in the service context |
| `gpu` | List of graphics adapters (DXGI); power from NVML when `nvml.dll` loads | DXGI, dynamic load of NVML | no, logged only (section 12, point 1) |
| `intel_dtt` | Intel DTT participants | presence of the `esifsvc` service | no, logged only |
| `pawnio` | The signed PawnIO driver, which gives programs the raw processor registers (among them the whole-platform energy counter that EMI does not show on the HP 250 G7) | whether PawnIO is installed, its version, and whether its device opens in this context. The scanner sends no command to the driver and loads no module. | no, logged only |
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

Details of the rules:

- The levels are tested in the order L1, L2, L3. A live `dram` channel without a live `cpu_package` channel does not give L2.
- "A battery that reports mWh": at least one battery with the unit mWh and a known full charge. With several batteries, `unit` is `mWh` when any of them is, `rate_known` is true when any of them reports a rate, and `full_mwh` is the sum of the known ones.
- Status of a read: `Unavailable` gives `present: false`; `Denied` gives `present: true, readable: false`; any other failure leaves `present` and `readable` out.
- Bytes that are not valid UTF-8 in a name become `?`. Sample numbers have at most 3 decimals.

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

The other sources use the same four common fields (`present`, `readable`, `live`, `error`) and these extra fields:

| Source | Extra fields |
|---|---|
| `acpi_power_meter` | `instances` (count), `sample` (largest value read, in the unit of the counter) |
| `display` | `brightness_pct` (only when readable) |
| `gpu` | `adapters`: list of `{ "name", "vendor_id", "memory_mb", "software" }`; `nvml`: `ok`, `unavailable` or `error`; `nvml_devices` (count) |
| `intel_dtt` | none (`present` says whether the service exists) |
| `pawnio` | `version` (only when known). `present` says whether PawnIO is installed; `readable` says whether its device opened. |

The sources appear in this order: `emi`, `battery`, `acpi_power_meter`, `display`, `gpu`, `intel_dtt`, `pawnio`. A field with an unknown value is left out.

`context`: the value of `--context`. Without the flag it is `admin` when the process is elevated, else `user`. With context `service` the files are named `capabilities.service.json` and `scan.service.log`.

Both files are written to a temporary file and renamed.

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

- A failed read or a counter that went backwards: that value is unknown for the interval.
- The service takes a first read of every counter at its start. Nothing is reported for the time before that read.
- A battery change that is physically impossible is not believed: the charge rose while the computer runs on battery, or the battery power over the period is above 200 W in either direction. The next reading is the new start value. The battery power of that minute is replaced by the power of the first following minute of the same interval whose change is possible (that power times the seconds of the replaced minute). The history row of the replaced minute is held back and written, with the replaced value, just before the row of that following minute. When no possible minute follows before the interval closes, the battery change of the held minute is unknown (an empty `battery_w`) and so is the battery change of the interval. Each impossible change is counted in the log with the word `battery_jump`; each replacement with `battery_replaced`. Why: on the HP 250 G7 the battery gauge jumps up by about 670 mWh when the charger is unplugged (measured 14 Nov 2026), which showed as +419 W in a 6-second history row.
- An unknown value needed for a field: the field is left out of the report. It is never sent as 0.
- L4, or L2 and L3 without a valid model: `energy_uj` is left out on mains. On battery section 4.1 applies in full.

### 4.4 Interval borders

- A change of the power source ends the interval at once and starts a new one. Each report then has one true `on_battery` value.
- Suspend ends the interval. Resume starts a new one. No report covers the time asleep.
- An interval shorter than 1 second is dropped.

### 4.5 Calculation details

- The service works in energy, not in watts. For each minute: measured energy = the growth of the EMI counters of the counted kinds, in µJ (1 pWh = 0.0036 µJ). Model energy = watts × seconds of the minute × 1 000 000.
- L1 in the service uses an EMI `system` channel only. The ACPI power meter is not used for energy, because its unit is not confirmed on any computer (section 12, point 6); a computer whose only whole-computer source is that meter is calculated at the next level down.
- Level of one minute: L1 when the `system` growth is known; else L2 when the model is valid and the growth of every kind in `measured_parts` is known; else L3 when the model is valid and the processor percent is known; else the minute is unknown.
- An interval with one unknown minute has an unknown `device_energy_uj`, so `energy_uj` and `device_energy_uj` are left out for it.
- Battery change of an interval = the sum of the battery changes of its minutes, after the replacement of section 4.3. Without an impossible change this equals the charge at its end minus the charge at its start. It is known only when the change of every minute is known, the unit is mWh and the number of batteries did not change.
- On mains: a battery is present but its change is unknown: `energy_uj` is left out. No battery is present: `energy_uj` = `device_energy_uj`.
- `cpu_percent` of an interval is the average over its minutes, weighted by their length.
- All energy values are rounded to the nearest whole µJ at the end, not per minute.

## 5. Service

### 5.1 Registration

- Name `EcoWattSenseService`, display name "EcoWattSense Energy Service", automatic (delayed) start.
- Account `NT AUTHORITY\LocalService`. If the scan in the service context shows a source denied that the user context can read, the account becomes LocalSystem; the main session reports that.
- Service SID type: unrestricted. Setup sets it with `ChangeServiceConfig2` (`SERVICE_CONFIG_SERVICE_SID_INFO`), so `NT SERVICE\EcoWattSenseService` is added to the service token and the folder rights of section 2.2 apply to it. The logon account stays as above.
- Recovery: restart after 60 seconds, three times.

### 5.2 Command line

- no argument: run under the service control manager.
- `--console [--data-dir <folder>] [--tick-seconds <n>] [--run-seconds <n>]`: run the same loop in a console, for tests. `--tick-seconds` (1 to 60, default 60) sets the length of one tick; a report is built every `interval_seconds / 60` ticks. `--run-seconds` stops the program by itself after that time. Both options exist only in console mode.
- `--data-dir <folder>` replaces `C:\ProgramData\EcoWattSense` in every mode.
- `--configure`: read `url=<...>` and `key=<...>` lines from standard input and write `service.ini` and `key.bin`. The key is never a command-line argument.
- Exit codes of `ews-service.exe`: 0 clean stop, or `--configure` done; 1 `--configure` failed, or the program was started without the service control manager and without `--console`; 3 bad command line (usage text on standard error); 4 start refused (data folder missing, `instance.lock` held or not usable, or device identity not readable or not writable); 5 the 60-second timer could not be made. Under the service control manager a code other than 0 is reported as a service-specific exit code (Windows exit code 1066). A pipe that cannot be created is not an exit code: the service runs on without it and logs `counted_error 10`.

### 5.3 Loop

- At start: report "running" to the service control manager first, then take `<data>\Client\instance.lock` (so the old client mode cannot report at the same time), read the device identity, run the scan with context `service` into `capabilities.service.json`, read the model.
- One waitable timer, 60 seconds. No other timer. The timer does not wake the computer.
- Each tick: read the cumulative counters (EMI channels, battery charge, `GetSystemTimes`), calculate the minute, append one history row.
- Every `interval_seconds` (default 300): build the report, append it to the queue, try to send, write `status.json`, check whether `model.json` changed (file time) and reload it.
- Events, no polling: power source change, suspend and resume, display on or off, session logon, logoff, lock and unlock (service control handler).
- Stop: take one last read, close the interval, put its report into the queue without sending, release the lock, exit within 5 seconds.
- If `instance.lock` is held by another program at start, the service writes one log line and stops.
- Read only at start: `service.ini`, `key.bin` and the device identity (`device-id.txt`). A change to them needs a restart of the service. `model.json` is the only file that is read again, at each report tick, when its file time has changed.
- Level without a valid model: when the scan gives L2 or L3 and `model.json` is missing or invalid, the level is L4. `status.json` shows `\"level\":\"L4\"` (without `model_generated_utc`) and the log has `level_change <old> 4`; an invalid file also logs `model_invalid` at each load, a missing file logs nothing more. In the history, a minute with no whole-computer reading (L1) and no valid model is unknown, so its `level` field is empty (it is never written as L4). A scan level of L1 needs no model and stays L1.
- Gap: a tick that comes more than 3 tick lengths after the last one (sleep or hibernate that the service did not see as suspend, or a clock set forward) or with the clock gone back is a gap. The service logs `gap_detected`, writes held history rows with empty battery fields, closes the interval at the time of the last good tick (that report is queued and sent as usual), and starts a new interval at the late tick. The time of the gap gets no history row and no energy. A tick of the normal length is never a gap.
- An interval whose processor percent is unknown gives no report (the server requires that field). It is counted in the log.

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

Formats: `timestamp` as `2026-11-14T10:05:00Z` (UTC, whole seconds). `interval_seconds` a whole number, at least 1. `cpu_percent` with one decimal, 0 to 100. `state` 2, 1 or 0. `screen_locked` and `on_battery` as `true` or `false`. `device_id` in lower case with hyphens. Energy fields are whole numbers; an unknown one is absent, never `null`.

### 5.5 Queue and sending

- `<data>\queue\reports.jsonl`: one report per line, oldest first, at most 2016 lines (7 days). When full, the oldest line is dropped and counted in the log.
- Send: `POST <server_url>/api/reports`, header `X-EWS-Key`, body a list of up to 100 reports, WinHTTP, 10 second timeout, no redirects. HTTPS only; HTTP is allowed only for loopback.
- 200 or 201: sent lines leave the queue. An item marked `rejected` leaves the queue and is counted.
- 401, 429, 5xx, network error: the queue stays. The next try waits 1, 2, 4, ... up to 60 intervals.
- Any other 4xx answer: the sent lines leave the queue and are counted as rejected, because a retry cannot fix them.
- The certificate of the server is always checked. The answer is read up to 64 KB; more is ignored.
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

`interval_seconds` is a multiple of 60 between 60 and 3600. A bad value falls back to the default and is counted in the log (`config_bad_values`). An empty `server_url=` (blank after trimming) means "server not set": it is not a bad value and is not counted. Without a URL nothing is sent: reports are queued, `last_send_result` is `disabled` and the log has `send_disabled 1` once. `server_enabled=0` gives the same words. A URL that is not valid (for example not HTTPS, and not HTTP to loopback) is a bad value and is counted. Server-side settings (`GET /api/settings`) are not read by the service.

`key.bin` is the server key protected with DPAPI (machine scope). The folder rights of `config\` are the real protection.

### 5.7 `status.json`

```json
{ "schema": 1, "service_version": "0.1.0", "level": "L2", "model_generated_utc": "2026-11-12T17:08:31Z",
  "last_report_utc": "2026-11-14T10:05:00Z", "last_send_utc": "2026-11-14T10:05:01Z",
  "last_send_result": "ok", "queued": 0,
  "server_url": "https://example.org:8443", "server_key_set": true, "server_enabled": true,
  "device_type": "laptop", "interval_seconds": 300, "idle_threshold_seconds": 600, "unused_threshold_seconds": 3600 }
```

`last_send_result` is one of: `ok`, `disabled`, `unauthorised`, `rejected`, `server_error`, `network`, `never`. Written to a temporary file and renamed.

At start the service reads the old `status.json` (at most 64 KB) and keeps `last_report_utc`, `last_send_utc` and `last_send_result` when each is valid (time in the form above and not more than 5 minutes in the future; one of the seven words); anything missing or invalid is left out, so a restart does not blank the client until the next report.

The service also writes the settings it uses, because the client cannot read `config\` (closed to users). They are the values read at start (5.3: config is read only at start), and they are always present:

- `server_url`: the URL in use, or `""` when there is none (missing or bad URL). JSON-escaped.
- `server_key_set`: `true` when `key.bin` was read and decrypted at start, else `false` (no file or not decryptable). The service reads the key for this even when sending is off. The key itself, its length and any hash of it are never written.
- `server_enabled`: `true` or `false` (`server_enabled` of `service.ini`).
- `device_type`: the value in use.
- `interval_seconds`, `idle_threshold_seconds`, `unused_threshold_seconds`: the values in use, whole numbers.

### 5.8 Log

`service.log` holds counts and fixed words only: no URL, no key, no file content, no server text. One line per start, stop, level change, send result and counted error. The file is capped at 1 MB; then it is renamed to `service.previous.log`.

## 6. Session pipe (client to service)

- Name `\\.\pipe\EcoWattSense.Service.Session`. The service is the server. Inbound only, message mode, remote clients refused. The code asks for a maximum of 4 instances but creates and serves only one at a time: it takes one client, reads one message (5 second time limit), disconnects, and listens again. A second client at the same moment finds the pipe busy and tries again later. The first instance is created with the "first instance" flag, so the service does not start listening when another program already owns the name (a counted error, the service runs on without the pipe).
- Access, as the security descriptor string `D:(A;;FA;;;SY)(A;;FA;;;BA)(A;;0x120196;;;IU)`: three allow rules and nothing else. SYSTEM (`SY`) and Administrators (`BA`) have full access. Interactive users (`IU`, anybody logged on at a screen or over remote desktop) have mask `0x120196`: write data, append data, write extended attributes, write attributes, read attributes, read control and synchronise. That is enough for a client to open the pipe and write to it, and not to read what other clients send. Other accounts, services and network users have no access.
- One message, ASCII, at most 128 bytes: `EWS1 idle=<seconds> bright=<0-100 or ->\n`
- The client sends one message every 60 seconds and one on each unlock.
- The service accepts a message only when the client's session is in the state "active" (the physical console or a remote desktop session; a user who works over remote desktop is not in the console session). A malformed or out-of-range message is dropped and counted.
- "Somebody is logged on" means: at least one session is active and has a user name. The service works this out from the session list at its start and again on every session event; it never polls.
- `screen_locked` is true when nobody is logged on, or when the active session is locked. With several active sessions the one that sent the last accepted message counts.
- `idle_seconds` of a report = idle value of the last message + seconds since that message, when the message is younger than 120 seconds. Without a fresh message: seconds since the last fresh message, or since the service start.
- `state`: 2 below `idle_threshold_seconds`, 1 below `unused_threshold_seconds`, else 0. Nobody logged on gives 0.
- Brightness: the service reads it itself when the scan in the service context says `display` is readable. Else it uses the value of the pipe. The client reads the brightness through WMI (`WmiMonitorBrightness` in `root\wmi`), the way the calibrator does; that works for a standard user.

## 7. History

- `<data>\history\<yyyy-MM-dd>.csv`, UTC date. Files older than 2 days are deleted at the change of day.
- First line: `v1,ts_utc,seconds,on_battery,state,idle_s,cpu_pct,device_w,measured_w,battery_w,level`
- One row per minute, appended, opened with shared read. An unknown value is an empty field.
- `device_w`: power of the whole computer. On battery it is the measured battery power, that is `battery_w` with the sign turned (Darek's decision, 14 Nov 2026); it is empty when the battery change of the minute is unknown. On mains it is the power of section 4.2 (measured or from the model). `measured_w`: the part read from hardware. `battery_w`: battery flow, negative when discharging.
- On battery, a minute whose charge did not fall (the gauge did not move) is held back. When the first minute with a fall follows, that fall is split over the held minutes and that minute in proportion to their seconds, and the rows are written in order with the split value in `battery_w` and `device_w` (Darek's decision, 14 Nov 2026). A row held for an impossible change (section 4.3) that waits behind such minutes gets the same split power. Rows held longer than 15 minutes, or still held when the power source changes, the computer sleeps, a gap is found or the service stops, are written with empty battery fields. The 15 minutes are measured from the start of the held minute (its end time minus its seconds) to the time of the minute being recorded now; the test runs each time a new minute is recorded, from the oldest held row on. The limit does not depend on the report interval: still minutes stay held across the end of a report interval (the report is sent at once and keeps the raw change). Known edge case: the same limit also covers a held impossible minute (section 4.3). With `interval_seconds` above 900, such a minute that waits more than 15 minutes is written with empty battery fields in the history, even when a possible minute follows later in the same interval and the report then uses the replaced value. History and report can then differ for that minute. This applies to the history only; the reports keep the battery change of section 4.5.
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
- `rest_w_per_cpu_pct`: growth of the rest (battery power minus the measured parts) between the phase "Rest, brightness 50%" and the phase "CPU 100%", divided by the difference of their mean processor use in percentage points. Both phases have the same brightness, so the screen power cancels out; against "screen off" the slope would hold about 1.4 W of screen power. 0 when it cannot be calculated.
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
- Power card: one card titled "Power". It shows the power now (last history row, "unknown" when none) in the large font, and under it two lines, "Today" (energy today, kWh) and "Source" (Mains, Battery or Unknown). The Last report card says "Waiting for the first report" until `status.json` has a report time.
- 24 h view: its own control. A line of `device_w` per minute for the last 24 hours from the history files, with a band for the state. Each state is shown with a word and a colour (Active, Idle, Unused). Gaps stay gaps.
- The view refreshes on a file change event of `history\` and `status\`, and only while the window is visible. Drawing is done by WPF shapes; no frame-by-frame redraw.
- Text fields, if any, are left-aligned with the label above or to the left.
- Send now: on the "Last report" card, under the report time, the line "Waiting to send: N reports" (`queued` of `status.json`; "All reports sent." for 0; no line when the field is missing) and the button "Send now", enabled when N > 0 and a server is set (`server_url` not empty and `server_key_set` true). A click writes `EWS1 send\n` to the session pipe, shows "Sending..." until the next `status.json` change and disables the button for 30 s; a failed write shows "The service did not answer."
- Calibrate button: on the "Calibrated" card, under the date of the model. It starts `EnergyCalibrator.exe` as the same user, without elevation. It is enabled only when none of these four reasons applies; the first one that applies is shown as a line under the button:
  1. a calibration is running: "Calibration is running."
  2. the calibrator was not found: "Calibrator not found."
  3. the computer is not on battery (or the power source is unknown): "Unplug the charger to calibrate."
  4. the charge is below 30 % or unknown: "Battery below 30 %."
  The state changes on power-mode events and on the end of the calibrator process; there is no timer.
- Measurement pulse: the dot on the Service card is green while the service runs and grey otherwise. When a history refresh brings a new last row, the dot pulses once (opacity and scale, about one second). The "Last measurement" line shows the end time of that row. No pulse when Windows client-area animations are off or the window is hidden.
- Donut "Energy by state (24 h)": on the Service card. A ring (about 110 px) split into Active, Idle and Unused by kWh with the total kWh in the centre; the card text has a legend of the three states with their percent. Without data it shows an empty grey ring and "No data". It is built from the history rows when they change; no timer.
- Settings page: a second item in the sidebar, "Settings", shown only in viewer mode. It is read-only and shows the values of `status.json` (section 5.7); a missing or wrong value shows as "unknown".
  - Server card: Address ("Not set" when empty), Key ("Server key is set" or "Server key is not set"; the key itself is never shown), Sending (On or Off), and the button "Change...", which starts `Setup.exe /settings` elevated (UAC). The note "Setup.exe not found." is shown, and the button disabled, when Setup.exe is not found.
  - This computer card: Device type, Report interval, Idle after, Unused after (in minutes).
  - About card: Service version and the data folder.
- Autostart box, on the "This computer" card of the Settings page: "Start the client with Windows". The task is "EcoWattSense Client" (section 10). The client reads it with `schtasks /query /tn "EcoWattSense Client" /xml`.
  - Group principal (a `GroupId` in the XML: the task made by Setup for every user): the box is ticked and disabled, with the line "Set for every user by Setup. Use Change... to switch it."
  - User principal (the per-user task of `Install-Autostart.ps1`) or no task: the box works as before. Ticking it runs `Install-Autostart.ps1` (needs the device type); unticking runs `Uninstall-Autostart.ps1`. No administrator rights are needed.
  - If schtasks gives no clear answer, the box is disabled with "The autostart setting could not be read."
  - Uninstall card (the last one): the line "Removes the programs and the service. Your data stays unless you choose to remove it." and the button "Uninstall EcoWattSense...", which starts `Setup.exe /uninstall` elevated (same lookup as "Change...") and, when Setup started, stops monitoring and closes the client; a cancelled UAC prompt keeps the client open; without Setup.exe the button is disabled with "Setup.exe not found.".

- Updates (level 1, viewer mode only): the Settings page has an "Updates" card above "Uninstall", and the About card has a "Client version" line.
  - The client reads `https://raw.githubusercontent.com/GCU-Sector-12/ecowattsense/EWS-energy-suite/client/spike/energy-suite/release/latest.json` (`{"version","zip_url","sha256","size"}`; HTTPS only, 10 s, at most 64 KB) at start and each time the Settings page opens; there is no timer. The file is untrusted: the version is digits and dots, `zip_url` starts with `https://github.com/GCU-Sector-12/ecowattsense/releases/download/`, `sha256` is 64 hex characters, `size` is 1 byte to 300 MB. The version is compared with the client's own (`AssemblyInformationalVersion` without the `+build` suffix) number by number.
  - The card shows "This version X" and one of "Up to date.", "Update available: Y" with the button "Update", or "Could not check for updates.". The first time a newer version is found (remembered in `%LOCALAPPDATA%\EcoWattSense\update-seen.txt`) one tray balloon says "EcoWattSense Y is available. Open Settings to update.".
  - "Update" downloads the ZIP to `%TEMP%\EcoWattSense-Update-<random>` with a progress line. Redirects are followed by hand and only to https on `github.com`, `objects.githubusercontent.com` and `release-assets.githubusercontent.com`. The size and the SHA256 must match, else the folder is deleted and the line is "The download is damaged. Try again.". The ZIP is unpacked with `%SystemRoot%\System32\tar.exe -xf` and `<folder>\EcoWattSense\Setup.exe /upgrade` is started elevated (UAC). When Setup started the client closes itself; a cancelled UAC prompt gives "Update was not started." and the client stays.
  - Before it starts, `Setup.exe` must carry a valid Authenticode signature (WinVerifyTrust; untrusted-root and chaining results are accepted) whose signer SHA-1 thumbprint equals `SigningPin.Thumbprint` (`GUI\Update`); otherwise the folder is deleted and the line is "The downloaded Setup is not signed by EcoWattSense. It was not started.". While the pin is `PENDING_THUMBPRINT` the check is skipped (`signature_check_skipped`).

## 10. Setup

Steps, in this order. Each step writes one line to `<data>\logs\install.log` (step, result word, error code). A failed step undoes the steps before it and ends with a clear message.

1. Check: Windows 11 x64, administrator rights.
2. Run `ews-scan.exe --context admin` into `<data>\scan`. Show the result: sources found, level, whether a calibration is needed. Write the same to the log.
3. Ask for the server address and the key. Fields are left-aligned. The key field is masked; when a key is already stored and the field is empty, its placeholder is "Server key is set". An empty key field keeps the stored key.
4. Copy the programs to `C:\Program Files\EcoWattSense\`.
5. Create the data folders with the rights of section 2.2.
6. Write the configuration through `ews-service.exe --configure` and the `device_type` from the scan.
7. Register and start the service.
8. Register the client autostart for every user: the Task Scheduler task "EcoWattSense Client", a log-on trigger with a one-minute delay and the principal group Users (interactive, not elevated), with the same design as `Install-Autostart.ps1`. It replaces the classic per-user task of that name: one task, for all users. No key is passed in its arguments.
9. When the scan says `needed`: offer to run the calibrator now. The user may skip it. The log records the choice and the calibrator exit code.

Test connection: on the Server page (install and settings) a button under the key field sends `GET <address>/api/settings` with the key (the typed one, or the stored one when the field is empty), with the rules of 5.5, and shows one result line; the log gets `test_connection <word>` only. Back and a click on a reached step in the sidebar work until the install has run; after that earlier steps are locked.

Computer name: the first field of the Server page (install and settings), empty by default, at most 60 characters, no control characters, spaces at both ends removed; prefilled from `<data>Clientdevice-name.txt`. After the service started (install) or the save (settings) Setup writes the name to that file (an empty name deletes it) and, when an address and a key are set, sends `PUT <address>/api/devices/<device_id>` with `{"device_name": <name or null>, "os": "windows", "device_type": ...}` under the rules of 5.5 (api-contract 5.3). A failure never fails the install: one line "The computer name could not be sent: <reason>." and the log `device_name <result word>`. The client Settings page shows the name in "This computer" ("Not set" when missing).

Settings (`Setup.exe /settings`): one page, started elevated (the client's "Change..." button does this). It has the server address, the server key (masked, as in step 3), the box "Send reports to the server", the device type (laptop or desktop) and the box "Start the client with Windows for every user". "Save" writes the values to the configuration and restarts the service ("Saved. The service was restarted.").

Uninstall (`Setup.exe /uninstall`): stop and delete the service, delete the task, delete the programs. The data folder stays unless the user ticks "Remove data". When it is started from `C:\Program Files\EcoWattSense\`, it first copies its folder to a temporary folder (`%TEMP%\EcoWattSetup-<guid>`, named in the code `EcoWattSenseSetup-<guid>`) and relaunches from the copy with `/uninstall /tempcopy`, because a program cannot delete the folder it runs from. The copy removes itself at the end.

Uninstall leaves no locked folder: the relaunched copy starts with the temporary folder as its working directory, and Setup moves its own current directory there before `delete_programs`. A program folder that stays locked (error 32) is retried 3 times 1 second apart, then marked for deletion at the next restart (MoveFileEx, MOVEFILE_DELAY_UNTIL_REBOOT; log line `delete_programs at_reboot 0`, finish page says so). A failure message names the steps done before it.

Closing the client: before the programs are copied (install, upgrade) and before they are deleted (uninstall), Setup closes every running `EcoWattSense.Client.Gui` and `EnergyCalibrator` process of any user (CloseMainWindow, wait up to 5 seconds, then Kill). It is its own step "Closing the client" (`close_client`, after `stop_old_service`); the log gets the line `close_client <count closed>`. A process that cannot be closed fails the step, and the undo applies.
Safe copy: Setup reads the service process id (QueryServiceStatusEx) before it stops the service, waits for STOPPED and then up to 30 seconds for that process to exit (log `wait_service_exit`; a timeout fails the step "The old service did not stop."), because the service control manager reports STOPPED before `ews-service.exe` is released.
The programs are copied as a swap: the package goes to `C:\Program Files\EcoWattSense.new` (log `copy_new`), then the current folder is renamed to `EcoWattSense.old` (sharing violation: 5 retries, 1 second apart) and `.new` takes its place (log `swap`), then `.old` is deleted, or marked for deletion at the next restart when it stays locked (log `remove_old`). A fresh install only renames `.new`. A failed copy or swap leaves the current folder as it was; the undo renames `.old` back when it still exists and deletes `.new`.
Progress: while installing, the Install page lists the steps in plain words, each with a mark (waiting, working, done, failed with its message), and one bar for the whole install; the copy step shows "Copying programs n of total". The steps report through IProgress on the UI thread, no timer; the list stays visible after the install and shows "The programs are installed in <program folder>."

Finish: the last page has the box "Start EcoWattSense now", ticked by default (install mode only). On Finish Setup starts the client from the program folder as the logged-on user, not elevated, without waiting; a failure shows one line, is logged as `start_client failed` and never fails the install.

Upgrade (`Setup.exe /upgrade`): only when the service EcoWattSenseService is registered and the program folder exists; otherwise Setup opens the normal install pages. It asks nothing: one page with the step list and the progress bar, and it keeps the configuration, the key, the device type and the computer name (it does not run `--configure`, the scanner or the name step). Steps, in this order: `close_client`, `stop_old_service`, `copy_programs`, `data_folders` (the rights of 2.2 again), `start_service`, `autostart_task` (device type read from `service.ini`). An undo on failure works as for the install, and every step is logged to `install.log`. At the end Setup starts the client as the logged-on user (a failure is a note, never a failure of the upgrade) and shows "Updated to <version>." with a Close button.

Server address argument: `Setup.exe /server=<url>` prefills the Server address field when no address is stored; a stored address wins. The value must start with `https://`, have at most 200 characters and hold no space, quote or control character; a bad value is ignored.

Version rule: one version for all programs. `Build-Package.ps1 -Version <v>` sets it: the C# programs get `-p:Version=<v>` (AssemblyInformationalVersion and FileVersion), the native programs get it through `Build-Native.ps1 -Version <v>` as the preprocessor value `EWS_VERSION` (`ews/version.h`: status.json `service_version`, first line of the `ews-scan` summary). Setup shows its version at the bottom of the sidebar. The package script also copies `README.md` next to `Setup.exe`, puts `EcoWattSense-Web-Setup.exe` (when built; `-SkipWebSetup` allows it missing) and its `.sha256` next to the ZIP, and writes `latest.json` = `{"version","zip_url","sha256","size","server_url"}` (`-ReleaseTag`, default `v<v>-energy-suite`; `-ServerUrl`, default `https://dev.ecowattsense.co.uk`). Clients read `latest.json` from the folder `release\`.

Icon: all programs of the suite use one icon, `assets\ecowattsense.ico` (a meter dial with a leaf), set as the application icon of the client, the calibrator and Setup. The tray icon of the client is taken from its own exe. The file is made by `scripts\New-AppIcon.ps1`.

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
3. Brightness through `\\.\LCD` is denied for a standard user on the HP 250 G7 (measured 13 Nov 2026). Whether the service account can read it is not tested yet. The pipe value is the fallback.
4. EMI on AMD and on other makers: known from literature only. The scanner on real laptops answers it.
5. Size and cost of the service (file size, memory, processor time): measured at the gate of session 2 with process counters and by comparing with the service stopped.
6. The unit of the ACPI "Power Meter" counter is not confirmed (it reads 0 on the HP 250 G7). Until a computer with a live meter is found, the service does not use it for energy.

## 13. Web installer

`EcoWattSense-Web-Setup.exe` (project `native/ews-websetup`, same output folder as `ews-service.exe`, `asInvoker`) downloads the package and starts `Setup.exe`. Code is in `ews-websetup`; the pure parts (manifest check, address and redirect check, size check, SHA256 compare, result words) are in `ews-core` (`web_setup.h`) with tests that use no network.

1. A TaskDialog "EcoWattSense setup" shows a progress bar and one status line. The work runs on a worker thread. Cancel stops it (exit code 1).
2. The manifest comes from the fixed address `https://raw.githubusercontent.com/GCU-Sector-12/ecowattsense/EWS-energy-suite/client/spike/energy-suite/release/latest.json` (WinHTTP, https only, certificate checked, 10 s timeouts, at most 64 KB, no redirect). Format: `{"version","zip_url","sha256","size"}` and the optional `"server_url"`. Every field is checked: `version` has digits and dots only; `zip_url` starts with `https://github.com/GCU-Sector-12/ecowattsense/releases/download/`; `sha256` is 64 hex digits; `size` is 1 byte to 300 MB. `server_url` must start with `https://`, be at most 200 characters, and hold no space, quote or control character; a missing or bad value is left out, not an error. When valid, `Setup.exe` is started with `/server=<url>`.
3. The zip goes to a new folder `%TEMP%\EcoWattSense-Setup-<random>` with progress. A redirect is followed only to https and only to `github.com`, `objects.githubusercontent.com` or `release-assets.githubusercontent.com` (at most 5 hops). The downloaded size must equal `size`.
4. The SHA256 is computed with BCrypt. A mismatch deletes the file and stops with "The download is damaged. Try again.".
5. `%SystemRoot%\System32\tar.exe -xf <zip> -C <folder>` unpacks it (exit code 0 required). `<folder>\EcoWattSense\Setup.exe` is started with ShellExecuteEx (it asks for UAC itself) and the installer closes. A cancelled UAC prompt shows "Setup was not started." and keeps the files.
6. A failure shows one clear line, with no address and no raw error text. The exit code names the step: 0 started, 1 cancelled, 2 manifest download, 3 manifest invalid, 4 temporary folder, 5 package download, 6 size differs, 7 SHA256 differs, 8 SHA256 not computed, 9 tar failed, 10 `Setup.exe` missing, 11 `Setup.exe` not started, 12 worker thread failed, 13 the signature of `Setup.exe` is missing, broken or not ours.
7. Before Setup.exe starts, its Authenticode signature is checked (WinVerifyTrust, no UI; `CERT_E_UNTRUSTEDROOT`/`CERT_E_CHAINING` are accepted while the computer does not trust our self-signed root yet) and the signer SHA-1 thumbprint must equal the pin `ews::kSigningThumbprint` (`ews/signature_check.h`). Any other result deletes the folder and shows "The downloaded Setup is not signed by EcoWattSense. It was not started." (exit code 13). While the pin is `PENDING_THUMBPRINT` the check is skipped (`signature_check_skipped` in the debug output).
