# Monitoring client

Runs on each computer. Reads idle time, CPU load and screen lock, decides Active / Idle / Unused and sends a report to the server every 5 minutes.

Layout (empty for now, filled after the stack decision in ADR-001):

```
ecowattsense_client/
  main.py        loop: read, decide state, send
  state.py       rules for Active / Idle / Unused
  reporter.py    HTTP POST /api/reports, retry, offline buffer
  config.py      settings from .env and command line
  platform/      one module per operating system behind a common interface
    base.py      abstract class: idle_seconds(), cpu_load(), is_locked(), power()
    windows.py   first target
    linux.py     second
    macos.py     last
tests/
```
