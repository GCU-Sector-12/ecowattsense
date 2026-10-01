# Spike: how WattSeal reads energy data

Source: https://github.com/Daminoup88/WattSeal (Rust, GPL-3.0), commit 651e51a, reviewed 30 Sep 2026.
Key files: `collector/src/sensors/cpu/{mod,estimation,linux_cpu}.rs`, `collector/src/sensors/cpu/windows_cpu/mod.rs`,
`collector/src/sensors/process.rs`, `collector/src/lib.rs` (loop), `doc/sensor_sources.md`.

## 1. Architecture
- Two binaries: **collector** (background, samples every 1 s, writes SQLite) and **ui** (reads SQLite). Optional MQTT publish.
- Every sensor implements `Sensor::read_full_data()` and returns **energy in microjoules (µJ) for the interval**, not watts.
  Power is derived later: `W = ΔE_uJ / duration_ms / 1000`.
- Collector loop (`lib.rs::run`): measure `since_last_update`, read all sensors, compute event, insert to DB, sleep to the next full second.
  Once a day: purge old rows and average them into longer periods (`common/src/database/purge.rs`).

## 2. CPU – three sources, picked at startup (`get_cpu_power_sensor`)
1. **Windows: RAPL via MSR** (WinRing0 driver, admin needed once).
   - Intel: MSR 0x606 (energy unit), 0x611 PKG, 0x639 PP0 (cores), 0x641 PP1 (iGPU), 0x619 DRAM.
   - AMD: 0xC001029B PKG, 0xC001029A CORE.
   - Energy unit = 1 / 2^(bits 8–12 of 0x606). Energy = counter × unit. Delta between two reads, 32-bit wrap handled (`wrapping_sub as u32`) on Intel.
2. **Linux: RAPL via sysfs** `/sys/class/powercap/intel-rapl:0/energy_uj` (needs root). Delta of the counter, wrap handled with `max_energy_range_uj`.
3. **Fallback = estimation** (no RAPL / no admin / **always on macOS**):
   - `usage = sysinfo global_cpu_usage()`
   - TDP from a hard-coded table by CPU name (`estimation.rs`, ~60 entries incl. Apple M1–M4), default 65 W.
   - `P = 0.2·TDP + (1.25·TDP − 0.2·TDP) · usage^1.6`  (idle = 20 % TDP, peak = 125 % TDP, exponent 1.6 from Google server paper)
   - `E = P × elapsed_seconds`.

## 3. Other components (all estimates, `doc/sensor_sources.md`)
- **GPU**: NVIDIA via NVML (mW, Win+Linux), AMD via ADLX (Win), Intel via PDH counters + PP1 energy (Win). Otherwise estimation.
- **RAM**: constant 5 W.
- **Disk**: SSD `0.05 W + 0.015 W per MB/s`, HDD `3 W + 0.035 W per MB/s`, unknown `0.3 W + 0.02 W per MB/s`.
- **Network**: `0.2 W + 0.01 W per MB/s`, capped 3 W per interface.

## 4. Per-app attribution (`process.rs`)
- `sysinfo` refresh of all processes; per-process CPU % divided by core count, GPU % from NVML/ADLX when available.
- `app_cpu_energy = (app_cpu% / total_cpu%) × cpu_energy`, same for GPU.
- `app_energy = (app_cpu_energy + app_gpu_energy) / (cpu_energy + gpu_energy) × total_system_energy`
  → if one app is the only user of CPU+GPU it gets 100 % of the whole machine's energy (by design, "encourage responsible usage").
- Grouped by process name (lower-case), sorted by energy.

## 5. Storage (SQLite, `common/src/database/migration.rs`)
Tables `cpu_data`, `gpu_data`, `ram_data`, `disk_data`, `network_data`, `process_data`; key `(timestamp, duration_ms)`;
columns `total_energy_uj`, `usage_percent`, bytes. Old rows averaged into bigger `duration_ms` buckets after 24 h.

## 6. What this means for EcoWattSense
- On macOS WattSeal has **no hardware reading at all** – only the TDP × usage^1.6 model. Our approach (state-based: Active/Idle/Unused × per-state watts) is a simpler version of the same idea; we can reuse their idle = 20 % TDP assumption and their TDP table as a starting point for per-device profiles.
- On Windows the only "real" number is CPU package energy via RAPL and it needs a kernel driver + admin – out of scope for our prototype; we should say so in D2 (limitation / future work).
- macOS alternative not used by WattSeal: `powermetrics` (needs sudo) or IOReport (private API) – possible future spike.
- Their per-app attribution is CPU-share based; our per-device attribution avoids that complexity.
- Units: store energy (µJ or Wh) per interval, not watts – makes aggregation over days trivial. Worth copying.
- Licence GPL-3.0: we may read and learn, not copy code into our repo.
