# Spike: macOS monitoring client (EcoWattSense)

| | |
|---|---|
| Status | **Done (research + throwaway PoC)** — needs a run on a real Mac to confirm numbers |
| Date | 25/09/2026 |
| Owner | Dariusz Jaroch |
| Time-box | ~half a day |
| Related | ADR-001 (Section 3, platform order), Milestone M1, risk "Idle detection" |
| Output | This note + `client/spike/macos_probe.py` (throwaway) |

## Question

Can the client read the four signals it needs — `idle_seconds`, `cpu_percent`,
`screen_locked`, `device_type` — on macOS **without a paid Apple Developer
account and without sudo**, running from source? (macOS is platform #3 in
ADR-001, so we only need to de-risk it, not ship it.)

Reminder: the client does **not** measure watts. It reports a state and the
four signals; the server estimates energy. So the spike is only about reading
those signals, not power metering.

## Answer (short)

Yes. All four are reachable from source with no account and no sudo. Only
`screen_locked` needs a pip install (`pyobjc`). Real power metering would need
sudo, but we do not need it — good, it confirms the state-based design.

## Findings per signal

| Signal | How on macOS | Account? | sudo? | Deps |
|---|---|---|---|---|
| `idle_seconds` | `ioreg -c IOHIDSystem` → `HIDIdleTime` (nanoseconds) | no | no | none (pure shell) |
| `cpu_percent` | `psutil.cpu_percent()` (fallback: parse `top -l 2`) | no | no | psutil (ADR choice) |
| `screen_locked` | Quartz `CGSessionCopyCurrentDictionary` → `CGSSessionScreenIsLocked` | no | no | `pyobjc-framework-Quartz` |
| `device_type` | `sysctl -n hw.model` ("book" → laptop) or battery presence | no | no | none |

### Notes
- **Idle** is the cheapest win: one shell call, zero dependencies, matches the
  ADR table. This is the macOS equivalent of Windows `GetLastInputInfo`.
- **CPU** is cross-platform via psutil — same line of code as Windows/Linux,
  which is the whole point of Option A. Stock macOS `python3` (3.9) has no
  psutil, so it must be pip-installed; the PoC has a `top` fallback so it still
  runs on a bare Mac.
- **Screen lock** is the only part needing a native binding (pyobjc). Cleanly
  isolated behind one function; returns `None` if pyobjc is absent so the client
  degrades instead of crashing.
- **Real power (out of scope):** `powermetrics` gives true CPU/GPU/package
  power in mW on Apple Silicon, but it **requires sudo**, so it is unusable for
  an unattended background client. This confirms we should keep the
  state-based estimation and not chase real wattage.

## Distribution & background run (confirms ADR)

- **From source** on a developer's own Mac: works, no Gatekeeper problem, no
  account. So Mac team members can test today.
- **Packaged `.app` for other people**: needs signing + notarisation →
  **paid Apple Developer account ($99/yr)**. Unchanged from ADR; this is why
  macOS stays platform #3.
- **Run every 5 min in background**: `launchd` LaunchAgent (`~/Library/
  LaunchAgents/*.plist`), no account, no sudo. Not built in this spike.

## Risks / open points

- Numbers not yet verified on the actual M4 Pro (spike written from the Linux
  sandbox VM, which cannot call macOS APIs). **Action: run the PoC on the Mac.**
- `CGSSessionScreenIsLocked` key is undocumented but stable for years; worth a
  fallback check in the real client.
- pyobjc adds ~1 dependency to the macOS build only — acceptable, isolated.

## How to run (on the Mac)

```bash
cd "~/_GCU/Group Project/Implementation/client/spike"
python3 macos_probe.py                       # idle + cpu work out of the box
pip3 install --user psutil pyobjc-framework-Quartz   # for accurate cpu + lock
python3 macos_probe.py                       # now screen_locked is populated
```

Expected: one JSON report in the D2 format (`os: "macos"`). Lock the screen and
re-run to see `state` flip to `unused`.

## Conclusion

macOS is low-risk for the client. Idle + CPU + device type need zero account
and zero sudo; lock needs one pip install. Nothing here blocks Option A. macOS
can safely stay platform #3, and Mac team members can help test from source
right away.
