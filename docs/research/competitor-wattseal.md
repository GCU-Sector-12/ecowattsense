# Competitor study: WattSeal (how it measures and estimates energy)

| | |
|---|---|
| Date | 25/09/2026 |
| Owner | Dariusz Jaroch |
| Source | https://wattseal.com and source code at https://github.com/vihithr/WattSeal (Rust, ~18k lines; `doc/sensor_sources.md`, `collector/src/sensors/cpu/estimation.rs`) |
| Feeds | G12-24 competitor comparison (MoSCoW), D2 energy model, D5 test plan |

## What WattSeal is

A **single-PC, real-time** power monitor with an on-screen overlay. Shows watts
per component and per application, plus cost and CO2. Samples every **1 second**.
Rust + Iced UI, SQLite kept locally. It is a diagnostic tool for one enthusiast,
not a fleet monitor. It has an MQTT publisher and its roadmap moves towards
sending data to a broker — i.e. towards our territory (many machines, central view).

## How it gets watts (hybrid: measure where possible, model elsewhere)

| Component | Windows | Linux | macOS |
|---|---|---|---|
| CPU | RAPL via MSR registers, needs **admin + kernel driver** (WinRing0) | RAPL via `/sys/class/powercap/.../energy_uj`, needs **sudo** | **Estimated** (no counters used) |
| GPU | NVML / ADLX / PDH | NVML only | none |
| RAM | constant **5 W** | 5 W | 5 W |
| Disk | `0.05 W + 0.015 × MB/s` (SSD), `3 W + 0.035 × MB/s` (HDD) | same | same |
| Network | `0.2 W + 0.01 × MB/s`, cap 3 W per interface | same | same |

Two things matter for us:

1. **Real CPU measurement needs elevated rights everywhere** (driver on Windows,
   sudo on Linux, nothing on macOS). Same wall we hit with `powermetrics`.
   A silent background client for a fleet cannot rely on it.
2. **Their macOS path and their fallback path are the same model**, and it is
   better than a flat "watts per state" table.

## The CPU estimation model (the useful part)

```
P_cpu = P_idle + (P_peak − P_idle) × usage^1.6
P_idle = 0.20 × TDP
P_peak = 1.25 × TDP
```

`usage` is CPU utilisation 0..1. TDP comes from a lookup table keyed by CPU name
(default 65 W). Exponent 1.6 is from a Google paper on server power (Fan et al.,
2007) and their own laptop measurements, validated against a smart plug.

Table entries relevant to us (W): Apple M4 Pro 40, M4 20, M3 Pro 36, M2 30,
M1 10; Intel 13xxU 15, 13xxP 28, HX 55; i5-12400 desktop 65; Ryzen 7840U 28.

Worked example, Apple M4 Pro (TDP 40 → idle 8 W, peak 50 W):

| CPU usage | P_cpu | + RAM 5 W + disk/net ≈ 0.3 W | 
|---|---|---|
| 5 % (idle desktop) | 8.4 W | ≈ 13.7 W |
| 30 % (normal work) | 14.1 W | ≈ 19.4 W |
| 100 % | 50 W | ≈ 55.3 W |

Energy: watts × seconds, stored in **microjoules** as an integer counter, summed,
converted to Wh (`J / 3600`) only for display. Cost = kWh × price/kWh, CO2 = kWh ×
g/kWh; both are user presets with a custom option.

## Per-application attribution (out of scope for us, but the idea)

`P_app = (app_cpu / total_cpu) × P_cpu + (app_gpu / total_gpu) × P_gpu`.
We deliberately do not collect per-process data (privacy: no window titles, no
process names).

## What EcoWattSense should take from this

1. **Upgrade the energy model from "watts per state" to the TDP curve.**
   We already collect `cpu_percent`, so the server can compute
   `P = 0.2·TDP + 1.05·TDP × cpu^1.6` per report at no extra cost on the client.
   The state (active / idle / unused) then only classifies energy as *useful*
   or *wasted*; it no longer has to guess the watts. Keep a flat table only as
   a fallback when the CPU model is unknown.
2. **Client sends the CPU model name once** (`hardware_info`, like WattSeal),
   e.g. `sysctl machdep.cpu.brand_string` on macOS, `platform.processor()` /
   WMI on Windows, `/proc/cpuinfo` on Linux. No privileges needed. Not personal
   data. Server looks up TDP from a small table (start with WattSeal's values).
3. **Add fixed platform constants** to the estimate: RAM 5 W, disk + network
   ≈ 0.3 W idle. Optional: screen on/off matters for laptops (WattSeal ignores
   it; we can use `screen_locked` as a proxy: locked → assume display off).
4. **Validation with a smart plug** goes into the D5 test plan: 1–2 machines,
   compare a day of estimates with the plug's kWh. Battery watts on MacBooks
   (see `spike-macos-client.md`) is a free second reference.
5. **Positioning for G12-24:** WattSeal = one PC, 1 s sampling, per-app, needs
   admin. EcoWattSense = fleet, 5 min sampling, no admin, privacy-first,
   "wasted energy" per organisation. Their MQTT roadmap shows the fleet need is
   real; our USP is that we start there.

## MoSCoW impact (proposal)

- Must: TDP-curve estimate on server; CPU model in hardware_info; cost + CO2 presets (UK grid ≈ 0.2 kg/kWh).
- Should: fallback flat table; display-off proxy; smart-plug validation (D5).
- Could: MQTT transport (already in "Future development").
- Won't: per-app attribution, GPU sensors, 1 s sampling, kernel drivers.
