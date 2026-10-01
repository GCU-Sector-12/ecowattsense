#!/usr/bin/env python3
"""
EcoWattSense - macOS client SPIKE (throwaway).

Goal of the spike: prove we can read the four signals the client needs
(idle_seconds, cpu_percent, screen_locked, device_type) on macOS WITHOUT a
paid Apple Developer account and WITHOUT sudo, running from source.

This is NOT production code. It has no scheduling, no retry, no config.
It prints one report to the console in the D2 report format.

Run on a real Mac:
    python3 macos_probe.py
Optional (better lock + battery detection):
    pip3 install --user psutil pyobjc-framework-Quartz
"""

import json
import os
import re
import subprocess
import sys
import uuid
from datetime import datetime, timezone

ID_FILE = os.path.expanduser("~/.ecowattsense_device_id")

# thresholds in seconds (spike values only; real ones go to D2)
IDLE_AFTER = 300      # 5 min  -> "idle"
UNUSED_AFTER = 1800   # 30 min -> "unused"


def device_id():
    """Random ID made once at first run (no user/host name -> privacy)."""
    try:
        with open(ID_FILE) as f:
            return f.read().strip()
    except FileNotFoundError:
        new = uuid.uuid4().hex[:8]
        try:
            with open(ID_FILE, "w") as f:
                f.write(new)
        except OSError:
            pass
        return new


def idle_seconds():
    """
    Time since last keyboard/mouse input.
    macOS: IOHIDSystem 'HIDIdleTime' in nanoseconds. Pure shell, no deps,
    no account, no sudo.
    """
    out = subprocess.check_output(
        ["ioreg", "-c", "IOHIDSystem"], text=True, stderr=subprocess.DEVNULL
    )
    for line in out.splitlines():
        if "HIDIdleTime" in line:
            ns = int(line.split("=")[-1].strip())
            return round(ns / 1_000_000_000, 1)
    raise RuntimeError("HIDIdleTime not found")


def cpu_percent():
    """psutil if present; else parse `top`. psutil is the ADR choice."""
    try:
        import psutil
        return round(psutil.cpu_percent(interval=1.0), 1)
    except ImportError:
        # fallback so the spike still runs on a bare Mac
        out = subprocess.check_output(
            ["top", "-l", "2", "-n", "0", "-s", "1"], text=True
        )
        idle = None
        for line in out.splitlines():
            if line.startswith("CPU usage"):
                idle = float(line.split(",")[-1].strip().split("%")[0])
        return round(100 - idle, 1) if idle is not None else None


def screen_locked():
    """
    macOS session lock via Quartz CGSessionCopyCurrentDictionary ->
    key 'CGSSessionScreenIsLocked'. Needs pyobjc. Returns None if not installed.
    No account needed.
    """
    try:
        from Quartz import CGSessionCopyCurrentDictionary
    except ImportError:
        return None
    d = CGSessionCopyCurrentDictionary()
    if not d:
        return None
    return bool(d.get("CGSSessionScreenIsLocked", 0))


def device_type():
    """laptop if a battery exists, else desktop. sysctl needs no deps."""
    model = subprocess.check_output(
        ["sysctl", "-n", "hw.model"], text=True
    ).strip()
    return "laptop" if "book" in model.lower() else "desktop"


def battery_watts():
    """
    Real instantaneous power flowing IN/OUT of the battery, in watts.
    macOS AppleSmartBattery: Amperage (mA, signed) x Voltage (mV).
    No sudo, no account. Laptops only (returns None on desktops / no battery).

    Caveat: this is battery flow, not wall draw. When plugged in and full,
    Amperage ~ 0, so it does NOT show total system consumption then. It is a
    real reading while the laptop runs on battery (discharging).
    """
    try:
        out = subprocess.check_output(
            ["ioreg", "-rn", "AppleSmartBattery"], text=True,
            stderr=subprocess.DEVNULL,
        )
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None

    def grab(key):
        m = re.search(r'"%s"\s*=\s*(-?\d+)' % key, out)
        return int(m.group(1)) if m else None

    amperage = grab("InstantAmperage")
    if amperage is None:
        amperage = grab("Amperage")
    voltage = grab("Voltage")
    if amperage is None or voltage is None:
        return None  # no battery -> desktop

    # Amperage is a signed 2's-complement 64-bit value; normalise.
    if amperage > 2**32:
        amperage -= 2**64
    watts = abs(amperage) / 1000.0 * voltage / 1000.0
    external = grab("ExternalConnected")
    return {
        "battery_watts": round(watts, 2),
        "direction": "discharging" if amperage < 0 else "charging/idle",
        "on_ac_power": bool(external) if external is not None else None,
    }


def state(idle, locked):
    if locked or idle >= UNUSED_AFTER:
        return "unused"
    if idle >= IDLE_AFTER:
        return "idle"
    return "active"


def main():
    if sys.platform != "darwin":
        print("This spike is for macOS (darwin). Detected:", sys.platform)
        print("Run it on the Mac (M4 Pro), not in the Linux sandbox.")
        return 1

    idle = idle_seconds()
    locked = screen_locked()
    report = {
        "device_id": device_id(),
        "timestamp": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "state": state(idle, bool(locked)),
        "idle_seconds": idle,
        "cpu_percent": cpu_percent(),
        "screen_locked": locked,   # None means pyobjc not installed
        "os": "macos",
        "device_type": device_type(),
        "power": battery_watts(),   # real battery watts, or None on desktop
    }
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
