# Windows 11 test: what to fix and what to measure

This file is for the person who tests the spike on a Windows 11 laptop and for the AI coding agent (for example Claude Code)
that works for them. It says what this spike is, what to fix and what to measure.

## What this is and why it may not work

EnergyCalibrator is a spike: a test tool, not part of the EcoWattSense client. It was written and run only once,
on 11 Nov 2026, on one laptop: HP 250 G7 Notebook PC, Intel Core i5-8265U, **Windows 10 Pro 22H2**.
It has never been built or run on Windows 11 or on another laptop. Expect problems: a build error, a reader that fails,
a phase that is always invalid. Your job is to make it work on this Windows 11 laptop, then measure.

The EcoWattSense client is installed and run by an administrator, so administrator rights are allowed.
The main open question is Windows 11 only: does Windows give processor energy (RAPL) through the Energy Meter Interface (EMI),
and how does it compare with Intel Dynamic Tuning (DTT) and the battery? On the Windows 10 test laptop EMI had no device.

## Read first

1. `../../../AGENTS.md`: the repository rules. They bind every change.
2. `README.md` in this folder: how the program works, the phases, the readers, the result files.
3. `docs/findings-2026-11-11.md`: everything found on 11 Nov 2026. Section 2 lists every measurement path,
   section 7 the proposals, section 8 the open points.
4. `docs/research/win11_rapl_emi.md` and `docs/research/emi_access_rights.md`: how EMI works and how to open it.
   `docs/research-report-en.md` is the short research report behind the findings.

## Rules for this work

- Change only files in `client/spike/energy-calibrator/`. Do not touch the client (`client/windows/`), the server,
  the dashboard or the contract (`shared/`).
- Flat code ("never nested"), as in AGENTS.md. Check with `pwsh -NoProfile -File Check-Nesting.ps1`.
- Every new function has a `///` documentation comment, every file a header comment (AGENTS.md).
- Run the program as administrator (the button "Restart as administrator"), so Intel DTT is read too.
  Do not install a third-party driver. Do not change a security setting or a WMI descriptor.
- A source that cannot be read is "unknown", never 0. Every failure (also error 5, access denied) is written once and the test goes on.
- No computer name, user name, serial number, device instance path or account id in any file you commit.
- Raw measurement files (`results\...`, `samples.csv`) are not committed. Commit only the short result summary described below.
- Before the first commit, ask the person you work for which YouTrack key to use (AGENTS.md: branch and commits start with the key).
  Work on a new branch from dev, never on dev.

## Steps

1. **Build.** `dotnet build -c Release` in this folder (.NET 10 SDK). Fix every error and warning. Run the nesting check.
2. **Probe EMI and Intel DTT as administrator** before the long test. Write down:
   Windows build, processor, laptop maker and model, whether it is a virtual machine, whether Intel DTT is installed.
   - Does the "Energy Meter" counter set have instances? (The program already reads it; see `LoadReaders.cs`.)
   - Open EMI directly: `SetupDiGetClassDevs` with the interface GUID `{45BD8344-7ED6-49cf-A440-C276C933B053}`,
     `CreateFile` with `GENERIC_READ`, then the four IOCTLs `0x224000` (version), `0x224004` (metadata size),
     `0x224008` (metadata), `0x22400C` (measurement). The units are picowatt-hours and 100 ns ticks:
     watts = 0.036 x change of pWh / change of ticks. List every channel name. Read `RAPL_Package0_PKG` for 60 s.
     On failure write the `GetLastError` code.
   - Enumerate the devices off the main thread (enumeration can hang on a slow ACPI call).
   - Do not add the channels together: PKG already holds the cores and the graphics.
   - Also note: is there a PSYS channel (whole platform)? What does `Get-PnpDevice` show for `SWC\VID8086_EMD_1.0`?
3. **Add EMI to the program** if step 2 found a device: a new optional source in the sample, the same way as the other readers,
   with the package energy per sample and a "package share" of the battery power per phase (as `cpu_share_pct` does for Intel DTT).
   If there is no EMI device, skip this step and say so in the summary.
4. **Run the full test** (about 18 minutes): charger unplugged, battery above 30%, other programs closed, no mouse or keyboard
   until the end. Phase 7 (screen off) failed on the Windows 10 laptop because the mouse was moved; it must be valid this time.
   If a phase is invalid, find the reason in `summary.txt`, fix the program if it is a bug, and run the test again.
5. **Write the result summary** as `docs/results-<maker>-<model>-<yyyy-mm-dd>.md`, in English, short sentences:
   machine (maker, model, processor, Windows build), EMI present yes or no (error code if it does not open),
   Intel DTT present yes or no, channel list, the power of each phase in watts, the model numbers from `model.json`,
   the package share per phase from EMI and from DTT, what failed and what you fixed.
   Add one line to the documents table in `README.md`.
6. **Commit and push** your branch, open a pull request into dev with the YouTrack key in the title, and say in the
   description what you changed, how you tested it and which Windows build you used.

## Done when

The program builds with no warning and passes the nesting check on Windows 11. One full test ran with all seven phases valid.
The result summary answers: does EMI work here, does Intel DTT work here, and what does this laptop use in each phase.

## If you are stuck

Stop and tell the person you work for what failed, with the exact error. Do not try a third-party driver
or another tool as a workaround. They will ask the team on Discord.
