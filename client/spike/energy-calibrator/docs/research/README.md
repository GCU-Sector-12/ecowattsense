# Research notes behind the energy reports

Notes written by research helpers on 11 Nov 2026. They are the sources of `../research-report-pl.md` and `../findings-2026-11-11.md`. Each note lists its own sources and gaps. Facts in them are read only unless the note says they were tested on the HP 250 G7 test laptop.

| File | What it covers |
|---|---|
| `win11_rapl_emi.md` | Since when and through which driver Windows 11 exposes processor RAPL energy through the Energy Meter Interface (EMI), Intel and AMD coverage, channel names, units, update rate, tools that read it. |
| `emi_access_rights.md` | Whether a normal user can open the EMI device and read the "Energy Meter" counters, how Firefox and Chromium read it, .NET versus PDH, and why the Intel Dynamic Tuning WMI class refuses a normal user. |
| `other_signed_paths.md` | Every other vendor-signed or built-in path on Windows 10 and 11: Intel PMT and IPF, Intel DTT, AMD Ryzen Master SDK, uProf and PMF, Dell, HP and Lenovo WMI, ACPI power meters, USB-C, PawnIO. |
| `rapl_accuracy.md` | How accurate RAPL and the battery gauge are compared with wall power, the processor share of laptop and desktop power, model-based tools and smart plugs. |
| `intel_energy_meter_driver.md` | The Intel "Energy Meter" driver (SWC\VID8086_EMD_1.0): package, versions, Windows versions, and whether it could give Energy Meter counters on Windows 10. |
