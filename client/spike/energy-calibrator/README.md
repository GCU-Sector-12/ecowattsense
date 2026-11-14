# EnergyCalibrator

A Windows desktop program (WPF, .NET 10) that runs a guided test of about 18 minutes on a laptop that runs on its battery.
It measures, from real readings only, how much power the whole laptop uses in seven states. Simple subtraction of those
powers gives a power model of the laptop: base power, screen power per brightness percent, CPU power per CPU percent, graphics power.

This is a spike: a test tool for the team, not part of the client. It was written and first run on 11 Nov 2026 on an
HP 250 G7 Notebook PC (Intel Core i5-8265U, Windows 10 Pro 22H2).

## Documents

| File | What it holds |
|---|---|
| `docs/research-report-en.md` | Research report (67 sources): which ways exist on Windows 10 and 11 to read real energy (Energy Meter Interface, battery, Intel Dynamic Tuning, E3/SRUM, drivers and tools), what needs administrator rights, accuracy, and what it means for the EcoWattSense client. |
| `docs/findings-2026-11-11.md` | Full record in English of everything found and measured on 11 Nov 2026, with all numbers from the test laptop and the source list. |
| `docs/research/` | The five research notes behind both reports, one file per topic; `docs/research/README.md` lists them. |
| `WINDOWS-11-TEST.md` | What to fix and what to measure when the spike is tested on a Windows 11 laptop, for the tester and their AI coding agent. |

## Build and run

```
dotnet publish -c Release -o out
out\EnergyCalibrator.exe
```

* Needs the .NET 10 Desktop Runtime on the laptop that runs the test. To avoid that, publish self-contained:
  `dotnet publish -c Release -r win-x64 --self-contained -o out-sc` (bigger, needs nothing installed).
* Put the folder somewhere you can write to (not Program Files): the results go to `results\<yyyyMMdd-HHmmss>\` next to the exe.
* The program runs as a normal user (manifest `asInvoker`). Only the Intel DTT reading needs administrator rights;
  the button "Restart as administrator (for Intel DTT)" restarts the program with the Windows permission prompt.
  The button is hidden when the program already runs as administrator.

## How to run the test

1. Unplug the charger. "Start" stays disabled until a reading shows that the laptop runs on battery
   (and the battery is above 30%, so the test cannot reach the 15% stop limit).
2. Close other programs. Do not touch the mouse or keyboard until the test is over.
3. Press Start and read the warning (battery use, power settings). OK starts the test, Cancel does nothing. The window shows the phase list with a progress bar for each phase. At the end the "Results" tab opens.

## The seven phases

Each phase lasts 150 seconds. The first 30 seconds are settling time and are left out of the result.
Both numbers are constants at the top of `TestSettings.cs`.

| # | Phase | What happens |
|---|-------|--------------|
| 1 | Rest, lowest brightness (screen on) | Brightness set to 0 (WMI WmiSetBrightness). The screen is still lit. |
| 2 | Rest, highest brightness | Brightness 100. |
| 3 | Rest, brightness 50% | Brightness 50: the reference for phases 4 to 6. |
| 4 | CPU 50% | Half of the logical processors run a busy loop (dedicated threads, below-normal priority). |
| 5 | CPU 100% | All logical processors run a busy loop. |
| 6 | Graphics | Full-screen borderless window with 200 shapes animated by Storyboards on RenderTransform plus BlurEffect. No busy CPU threads. |
| 7 | Screen off | The display is switched off with SC_MONITORPOWER, and the program checks that Windows reports it as off (see "How the display state is measured"). It comes back on at the end. Do not touch the mouse or keyboard. |

Phases 4 to 7 keep the brightness at 50% (phase 7 changes nothing: the display is off).

The program restores everything at the end, on Stop and after any error: the original brightness, the busy threads stop,
the graphics window closes, the display is switched on. The test also stops by itself when
the charger is plugged in during a phase (the phase is marked "Invalid: charger"), when the battery falls below 15%,
when the mouse or keyboard is used while the display is off (the phase is marked "Invalid: input"),
when the display does not turn off ("Invalid: display stayed on") or turns on again during the phase ("Invalid: display woke up").
Esc in the graphics window also stops the test.

## What is read every 2 seconds

Each source is optional. An unavailable source shows "not available: <reason>" and the test goes on.

| Source | Values | Notes |
|--------|--------|-------|
| Battery IOCTL (battery class driver) | power state, charge in mWh, voltage in mV, rate in mW | Works for a normal user. The rate is unknown on many laptops (it was always unknown on the HP 250 G7). The charge falls in steps of about 11 mWh every 10 s there. |
| Battery WMI (root\wmi BatteryStatus) | charge, voltage, charger flag | A second source. It lags the IOCTL by about one 10 s step. |
| GetSystemPowerStatus | battery percent, charger flag | Used for the 15% stop limit and the Start check. |
| Intel DTT (root\wmi EsifDeviceInformation) | power (W) and temperature of every participant | Needs administrator rights. The class is undocumented. "CPU package (Intel DTT)" is participant 3: its meaning is derived from a test on the HP 250 G7 (1 to 5 W at rest, 23 to 27 W under full load); participant 4 behaved like the cores. Only the participant number is logged, never the full instance name. |
| CPU use (GetSystemTimes) | total over all logical processors | |
| Performance counters | % Processor Performance, Processor Frequency, Thermal Zone Temperature (Kelvin to Celsius), GPU Engine 3D, disk bytes/s, network bytes/s | English counter names. A rate needs two reads, so the first sample has no counter values. |
| Screen brightness (WMI WmiMonitorBrightness) | percent | Works on laptops with a built-in panel. |
| Energy Meter counter set | raw Energy and Time of every instance, and watts of package, cores, graphics and memory | Windows 11 on supported processors only; "not available" elsewhere. See "Windows 11 Energy Meter" below. |
| Power settings (powrprof.dll) | energy performance preference, boost mode, maximum and minimum processor state on battery, Windows power mode | Works for a normal user. See "Power settings during the test". |
| Last user input (GetLastInputInfo) | idle seconds | Used to detect input during the "Screen off" phase. |
| Display state (power setting notification) | off, on, dimmed or unknown | Column `display_state`. See the next section. |

GPU 3D use: the instances of "GPU Engine" whose name holds `engtype_3D` are grouped by engine (the name without the process id),
summed over all processes for each engine, and the busiest engine is taken (at most 100%).

A value that cannot be read, or that is outside a believable range, is left empty ("unknown"). It is never replaced by 0.

## Windows 11 Energy Meter

Windows 11 (measured on build 26300) shows processor energy in the performance counter set "Energy Meter". It needs no administrator
rights: a normal user reads it with `new PerformanceCounter("Energy Meter", "Energy" | "Time", instance, readOnly: true).RawValue`.

* Instances: `RAPL_Package0_PKG` (the whole processor package), `RAPL_Package0_PP0` (the cores), `RAPL_Package0_PP1` (the integrated graphics),
  `RAPL_Package0_DRAM` (the memory), and `_Total`. PKG already holds PP0 and PP1, so the values are never added up.
* Units in this counter set: `Energy` is cumulative picowatt-hours. `Time` is cumulative **milliseconds** (not 100 ns).
* The `Power` counter is not watts (its raw value equals the cumulative energy), so it is not used.

```
watts = (Energy2 - Energy1) in pWh / (Time2 - Time1) in ms x 3.6e-6
```

Check: 9,637,423,889 pWh over 5,026 ms is 6.90 W. A power needs two reads, so the first sample has empty `emi_*` columns.
A missing set or instance, a counter that did not move forward, or a value above 500 W is left empty (unknown), never 0.

`samples.csv` has the fixed columns `emi_pkg_w`, `emi_cores_w`, `emi_gpu_w` and `emi_dram_w` (watts since the previous sample). The raw
`energymeter_<instance>_energy` and `_time` columns stay in the final file. `phases.csv` has `mean_emi_pkg_w`, `mean_emi_cores_w`,
`mean_emi_gpu_w`, `mean_emi_dram_w` (means over the used samples) and `emi_package_mwh` (the package energy of the used samples in mWh:
the sum of the energy steps divided by 1e9), to be read next to `battery_drop_mwh`. The CPU share (`cpu_share_pct`) uses the package
power of the Energy Meter when it was read, otherwise Intel DTT; `summary.txt` names the source for each phase.

## Power settings during the test

On battery, Windows can hold the processor back, so a "CPU 100%" phase may not reach full power (on 12 Nov 2026, on a Windows 11 laptop,
100% CPU use gave only 92% processor performance and 7.3 W package power, instead of about 24 W). So the program sets the processor
power settings for battery use to maximum performance for the test, and puts them back afterwards.

* **Warning first.** Press Start and a box says that the test runs about 18 minutes on battery and drains it at full power, with an
  estimate of the battery share it uses (18 minutes at about 15 W against the full charge capacity of the battery, rounded; the
  number 15 W is `FullPowerEstimateWatts` in `TestSettings.cs`). It also says that the power settings are changed and put back, and asks not to
  plug in the charger or close the lid. Cancel starts nothing and changes nothing.
* **What is read.** The live row "Power settings on battery" shows, for the active power plan (powrprof.dll, documented functions
  `PowerGetActiveScheme` and `PowerReadDCValueIndex`) the energy performance preference (PERFEPP), the boost mode (PERFBOOSTMODE),
  the maximum and minimum processor state (PROCTHROTTLEMAX, PROCTHROTTLEMIN), and the Windows 11 power mode
  (`PowerGetEffectiveOverlayScheme`). The same values are in `samples.csv` (`power_epp`, `power_boost_mode`, `power_max_state_pct`,
  `power_min_state_pct`, `power_mode_code`: 0 Balanced, 1 Best power efficiency, 2 Better performance, 3 Best performance, 4 other).
* **What is changed.** On OK, the program first saves the original values (plan GUID, the four battery values and the power mode) to
  `power-settings-backup.json` next to the exe. Only then it writes the battery values `PERFEPP = 0`, `PERFBOOSTMODE = 2` (Aggressive),
  `PROCTHROTTLEMAX = 100` into the active plan (`PowerWriteDCValueIndex`) and applies them (`PowerSetActiveScheme` on the same plan).
  The minimum state is kept. The power mode overlay is only recorded: changing it needs an undocumented API, which the program does not use.
  The charger (AC) values are never touched.
* **When it is put back.** At the normal end, on Stop, after any error in the run (the existing clean-up path of `TestRunner`), and when the
  window closes. The program writes the original values back, reads them again, compares, and only then deletes the backup file.
  If the plan was changed by the user during the test, the values are written to the original plan but that plan is not made active again.
* **After a crash.** If the program was killed during a test, `power-settings-backup.json` is still there. At the next start the program
  puts the settings back from it first and says so in the status line. If that fails, the file stays and the status line says so.
* **If the change fails** (for example access denied), a box shows the reason, whatever was written is put back, and the test runs with the
  settings unchanged. `summary.txt` says so in "Power settings of the processor on battery" and in the warnings.
* **In the results.** `summary.txt` and `model.json` (`power_settings`) hold the settings before the test, the settings used and whether
  they were restored (read back and compared). The plan GUID is not written to the result files. The "CPU 100%" phase gets a warning when its mean
  processor performance (`mean_cpu_performance_pct` in `phases.csv`) is not above 100: "the processor did not boost; power settings or the
  laptop maker's power limits held it back".

## How the display state is measured

The program does not trust that the display-off command worked. It registers the main window with
`RegisterPowerSettingNotification` for `GUID_CONSOLE_DISPLAY_STATE`. Windows then sends `WM_POWERBROADCAST` (`PBT_POWERSETTINGCHANGE`)
at once and at every change; the data is a DWORD: 0 off, 1 on, 2 dimmed. The latest value is written into every sample as `display_state`
(off, on, dimmed, or unknown before the first notification) and shown in the live readings. In the "Screen off" phase the settling time
starts when the state first reports off. If it does not report off within 10 seconds of the first command, the phase is "Invalid: display
stayed on" and the test goes to the clean-up. If a later sample shows on or dimmed, the phase is "Invalid: display woke up" and its samples
are kept. `phases.csv` has `display_off_pct`, the share of the used samples with the display off, and `summary.txt` shows it for each phase.

## How the power of a phase is worked out

Only samples after the settling time are used.

```
battery power (W) = (charge at the first sample - charge at the last sample) in mWh x 3.6 / seconds between them
```

The charge comes from the IOCTL; the same calculation with the WMI charge is a check (column `battery_w_wmi`).
When the IOCTL gives no charge, the WMI value is used and the phase gets a warning.

The battery charge changes in coarse steps. A phase whose drop is below 100 mWh gets a warning: its power is rough.
At 4 W a phase of 120 s drops only about 133 mWh, so one step (about 11 mWh) is a few percent. Differences between phases that are
smaller than that are noise. If the numbers look noisy, make `PhaseSeconds` longer.

## The model (simple subtraction, no regression)

```
base_w            = P(Screen off)
screen_w_per_pct  = (P(highest brightness) - P(lowest brightness)) / 100
screen_w_at_50    = P(brightness 50%) - P(Screen off)
cpu_w_per_pct     = (P(CPU 100%) - P(brightness 50%)) / (cpu%(CPU 100%) - cpu%(brightness 50%))
                    and the same with the CPU 50% phase, to show how linear the CPU power is
graphics_w        = P(Graphics) - P(brightness 50%)
cpu_share         = Intel DTT package power / battery power, for every phase (when DTT is available)
```

A value that needs a phase that is not complete (or a CPU use difference below 5 points) is shown as "not available" with the reason.
Note: on a processor with hyper-threading, "half of the logical processors" busy is not half of the full power; that is why the model
divides by the measured CPU use of each phase and not by 50 or 100.

## Data is saved during the run

`RunRecorder.cs` writes the header of `samples.csv` and `phases.csv` when the run starts, appends a row to `samples.csv` after every
sample, and appends a row to `phases.csv` when a phase ends. If the program is killed, everything measured so far is on disk.
The growing `samples.csv` has the fixed columns only. When the run ends normally the program writes `samples.csv` again with the extra columns
(DTT, thermal zones, Energy Meter), and always writes the final `phases.csv` (all seven phases), `model.json` and `summary.txt`.
A file error is shown in the status line and never stops the test.

## Result files (`results\<yyyyMMdd-HHmmss>\`)

* `samples.csv`: every sample, every source as a column, empty cell = unknown. `display_state` is off, on, dimmed or unknown. Phase 0 means "no phase was running".
  Fixed columns include `emi_pkg_w`, `emi_cores_w`, `emi_gpu_w`, `emi_dram_w` (Windows Energy Meter watts).
  Extra columns are `dtt_p<n>_power_w`, `dtt_p<n>_temp_c` (Intel DTT participants), `thermal_zone_<n>_c`, `energymeter_<name>_energy` and `_time`.
* `phases.csv`: one row per phase with the numbers above and a warning column.
* `model.json`: the model, the power of each phase and the CPU share per phase.
* `summary.txt`: the same in plain English.

The files hold the PC maker, model and processor name. They hold no computer name, no user name, no serial number and no device id.
Numbers use a dot as the decimal mark.

## The model file for the service (`<data>\calibration\model.json`, schema 2)

When a test ends, `ServiceModel.cs` writes the model that the EcoWattSense service reads (SPEC section 8). `<data>` is
`C:\ProgramData\EcoWattSense`; `--data-dir <folder>` replaces it (used by the tests). The `results\...` folder beside the program is written as before.

* Fields: `schema` 2, `generated_utc` (UTC, whole seconds, Z), `machine`, `measured_parts`, `model` (`base_w`, `rest_w`, `rest_w_per_cpu_pct`,
  `cpu_w_per_pct`, `screen_w_at_50`, `screen_w_per_pct`, `graphics_w`), `phase_power_w`, `cpu_share_by_phase`, `warnings`.
* `measured_parts`: the Energy Meter kinds that gave a value in the test: `cpu_package` (the package channel), `dram`. Empty when none; then
  `rest_w` and `rest_w_per_cpu_pct` are 0 and the service serves level L3 only.
* `rest_w`: battery power minus the measured parts in the "Screen off" phase (`RestModel.cs`).
* `rest_w_per_cpu_pct`: growth of that rest per percent of CPU use. It is taken between the "CPU 100%" phase and the "Rest, brightness 50%"
  reference phase (same brightness), so the screen power does not leak into it. A fall is noise and gives 0. 0 also when it cannot be worked out.
* Limits (the same as in the service, checked here first): watt values 0 to 500, per-percent values 0 to 5, file at most 64 KB. A value outside,
  or unknown, writes nothing (exit code 3).
* Before writing, the current file is copied to `model.previous.json`; the new text goes to `model.json.tmp` and is renamed to `model.json`.
* Exit codes: 0 model written; 1 stopped by the user (or the window closed with the start conditions met); 2 start conditions not met
  (no battery, charger connected, charge below 30%) when the window is closed without a test; 3 test aborted, or a value outside the limits;
  4 the file cannot be written.
* Tests: `pwsh -NoProfile -File tests\Test-Calibrator.ps1` builds and runs `tests\CalibratorTests` (exit code = failed checks). It uses
  `tests\data\phases-20261112.csv`, the phases of the real run of 12 Nov 2026 (no serial number, user name or computer name in it).

## Things the program does that the test needs (not asked for in the first brief)

* It asks Windows to keep the system and the display awake (SetThreadExecutionState) during the test. Without this, Windows would dim
  and switch off the display after a few minutes on battery and spoil the phases. The request ends with the test; no setting is changed.
  While the display is off on purpose, only the system is kept awake.
* It watches the last user input during the "Screen off" phase; input there switches the display on and invalidates the phase.
* The display-off command (`SC_MONITORPOWER`) is sent with `SendMessage` to the program's own main window, 2 seconds after the graphics
  window has closed. Only if the display is not off 3 seconds later, it is sent once more to all windows with `SendMessageTimeout`
  (so one window that does not answer cannot freeze the program). The display must report "off" within 10 seconds of the first command.
* The Start button also needs a battery level of 30% or more.

## Code rules and the nesting check

The code follows the rule "never nested": no `if`, loop, `switch` or `try` inside another control statement, and no lambdas with logic.
Check it with:

```
pwsh -NoProfile -File Check-Nesting.ps1 -SelfTest   # shows that the checker finds nesting in small examples
pwsh -NoProfile -File Check-Nesting.ps1             # checks all .cs files in this folder
```

## Files

* `TestSettings.cs`: all numbers (phase length, settling time, limits, graphics load).
* `PhaseModels.cs`: the seven phases. `TestRunner.cs`: runs them and restores everything.
* `SampleCollector.cs`, `BatteryIoctlReader.cs`, `WmiReaders.cs`, `LoadReaders.cs`, `NativeMethods.cs`: the readers.
* `CpuLoad.cs`, `DisplayControl.cs`, `GraphicsWindow.xaml(.cs)`: the loads and the display control.
* `PowerSettingsApi.cs`, `PowerSettingsSession.cs`: read, change and restore the processor power settings, the backup file and the start warning.
* `Analysis.cs`: per-phase results and the model. `RestModel.cs`: rest power. `ResultWriter.cs`: the result files.
* `ServiceModel.cs`: the schema 2 model file for the service, its limits, the exit codes and `--data-dir`. `tests\`: the console tests.
* `MainWindow.xaml(.cs)`, `ViewModels.cs`: the window.
