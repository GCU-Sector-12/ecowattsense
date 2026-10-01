# ADR-001: Technical stack for EcoWattSense

| | |
|---|---|
| Status | **Proposed** (to confirm at the week 3 team meeting) |
| Date | 25/09/2026 |
| Owner | Dariusz Jaroch |
| Decides | Programming language, framework, database, IDE, diagram tools, testing |
| Related | Planning Report, Section 5 (Table 6). Milestone M1 (week 3). Risk "Skills gap" and "Idle detection" |

## 1. Context

EcoWattSense has three parts (Planning Report, Section 2):

1. **Monitoring client.** Runs on each computer. Every 5 minutes it reads idle time, CPU load and screen lock, decides Active / Idle / Unused and sends a report.
2. **Server.** Receives reports, stores them in a database, gives data to the dashboard.
3. **Web dashboard.** List of computers, state, history, estimated energy.

The client is the hard part. It must read data from the operating system (Windows first, see Section 2). The server and dashboard are a normal small web application.

The team has seven members with mixed experience. Most of us are new to at least one part. So the stack should be small, free, and easy to learn.

## 2. Platform order for the client

Decided by Dariusz Jaroch, 25/09/2026. We build the client for one system at a time:

1. **Windows** first. Most office and lab computers run Windows, so this is the main target.
2. **Linux desktop** second, only when the Windows client works end to end.
3. **macOS** last. To give a packaged app to other people on macOS, it must be signed and notarised, and this needs a paid Apple Developer Program account. Without it, macOS blocks the app (Gatekeeper). Running the Python script from source on a developer's own Mac still works, so Mac users in the team can help with testing.

The server and dashboard do not depend on the OS. Team members with a Mac can work on them from day one.

## 3. What the client needs from the OS

This table decides most of the language choice.

| Data | 1. Windows | 2. Linux desktop | 3. macOS |
|---|---|---|---|
| Time since last input | `GetLastInputInfo` (user32) | X11: XScreenSaver extension (`xprintidle`). Wayland: no common API, GNOME `org.gnome.Mutter.IdleMonitor` or KDE `org.freedesktop.ScreenSaver` over D-Bus | `ioreg -c IOHIDSystem` → `HIDIdleTime` |
| CPU load | `psutil.cpu_percent()` | `psutil.cpu_percent()` | `psutil.cpu_percent()` |
| Screen locked | session / input desktop check | `loginctl show-session ... -p LockedHint` | `CGSessionCopyCurrentDictionary` |
| Run in background | Task Scheduler or service | `systemd --user` service | `launchd` agent |
| Distribution | `.exe` (PyInstaller), no account needed | script or package, no account needed | signed and notarised app, **Apple Developer account** |

How each language handles this:

- **Python.** `psutil` covers CPU on all three systems. `ctypes` reaches Windows APIs without a compiler. D-Bus and shell calls work on Linux, `pyobjc` on macOS.
- **Node.js.** `os.loadavg()` returns 0 on Windows, so CPU needs the `systeminformation` package. Idle time needs a native add-on or a shell call.
- **C# (.NET).** Very good on Windows (P/Invoke). Linux and macOS need extra work.

Idle time and screen lock need different code on each OS in every language. The code is split into one module per OS behind one common interface, so adding Linux and macOS later does not change the rest of the client. The week 3 spike tests **Windows only**.

**Linux risk:** on Wayland there is no single idle API. We support X11 and GNOME first and treat other desktops as "best effort".

## 4. Options

### Option A: Python everywhere (recommended)

- Client: Python 3.12+, `psutil`, `ctypes` (Windows), `pyobjc` (macOS), `requests`.
- Server: **FastAPI** + **SQLModel** (SQLAlchemy). FastAPI checks the JSON of each report and makes API documentation automatically (`/docs`), which helps the dashboard team and the D2 document.
- Dashboard: HTML pages served by the server, **Chart.js** for charts, a small CSS framework (Pico.css or Bootstrap). No build step.
- Tests: **pytest**, FastAPI `TestClient`.

For: one language for the whole team, one set of tools, easy pairing across parts. Strong libraries for the client.
Against: a dashboard without a JS framework is less "modern". Packaging the client as an `.exe` needs PyInstaller (only needed after the prototype).

### Option B: Python client + Node.js server and dashboard

- Client as in A. Server: Express. Dashboard: React (Vite) + Chart.js.

For: React is a useful skill for CVs. Good if several members already know JavaScript.
Against: two languages, two test tools (pytest + Jest), two sets of setup problems. The report format must be kept in sync by hand.

### Option C: C# / .NET

- Client: .NET worker service. Server: ASP.NET Core. Dashboard: Blazor.

For: one language, strong typing, Visual Studio.
Against: strong on Windows, but Linux and macOS clients need extra work. Heavier setup. Only an option if most of the team already knows C#.

## 5. Other choices

| Area | Proposal | Reason |
|---|---|---|
| Database | **SQLite** in the prototype, through SQLModel | One file, no server to install, enough for 7 laptops sending every 5 minutes (about 2,000 rows a day). SQLModel lets us move to PostgreSQL later without rewriting queries. |
| Protocol | **HTTP + JSON (REST)**, `POST /api/reports` | Simple to test with a browser or curl. MQTT stays in "Future development". |
| IDE | **VS Code** as the team standard | Free, runs on Windows, Linux and macOS, Python and JS extensions, Live Share for pairing. Members may use PyCharm (free for students) if they prefer. A shared formatter (`ruff`) keeps code the same whatever the IDE. |
| Version control | GitHub organisation GCU-Sector-12 (decided) | Branch protection on `main`, one review per pull request. |
| Diagrams | **Mermaid** in Markdown for architecture, sequence and ER diagrams. **draw.io** for UML use case and activity diagrams | Mermaid is text, so it lives in the repo and shows on GitHub. Mermaid has no use case diagram, so draw.io (free, `.drawio` files in the repo) covers that. |
| Wireframes | **Figma** (free education plan) or paper sketches | The dashboard team chooses. Export PNG to `docs/wireframes/`. |
| Testing | pytest + a manual test plan (D5) | Formal decision in week 5, as in the Planning Report. |

## 6. First draft of the report format

This is here to show the client and server are simple. It will move to the design document (D2).

```json
{
  "device_id": "a1b2c3d4",
  "timestamp": "2026-10-05T14:05:00Z",
  "state": "idle",
  "idle_seconds": 912,
  "cpu_percent": 3.5,
  "screen_locked": false,
  "os": "windows",
  "device_type": "laptop"
}
```

No user name, no host name, no window titles. `device_id` is a random ID made at install time (privacy risk in the risk register).

## 7. How we decide

1. Week 3: each member fills in the skills checklist. We look at Python, JavaScript and C# answers.
2. Week 3: spike on idle detection on Windows (Ruslan, Dariusz), in Python first. If it works, Option A or B is safe.
3. Team meeting: choose A, B or C. If there is no clear winner, **Option A** is the default because it has the fewest tools.

| Criterion | Weight | A | B | C |
|---|---|---|---|---|
| Client can read idle / CPU / lock (Windows now, Linux and macOS later) | 3 | 3 | 2 | 2 |
| Team already knows it (from the checklist) | 3 | ? | ? | ? |
| Few tools to learn | 2 | 3 | 2 | 2 |
| Free and runs on all team laptops | 1 | 3 | 3 | 2 |
| **Total** (fill in after the checklist) | | | | |

Scores 1 (poor) to 3 (good). The "?" row is filled in from the skills checklist results.

## 8. Decision

_To be filled in after the week 3 meeting: chosen option, date, who was present._

## 9. Consequences

- Update Table 6 in the Planning Report and the D2 document.
- Update the platform order where the Planning Report says "Windows and macOS": Phase 2 activities, milestone M1, risk register (idle detection), Table 5 spike task, learning paths.
- Create the repository structure: `client/`, `server/`, `dashboard/`, `docs/`.
- Update the learning paths (Section 5) with the chosen tools.
- Close the YouTrack task "Confirm the technical stack".
