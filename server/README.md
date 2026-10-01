# Server

Receives reports from the clients, stores them in a database and serves data to the dashboard.

Layout (empty for now, filled after the stack decision in ADR-001):

```
ecowattsense_server/
  main.py        application entry point
  api/           routes: reports, devices, energy
  models.py      Device, Report, EnergyProfile
  db.py          connection and migrations
  energy.py      energy model: state x wattage -> Wh
  schemas.py     report format, see shared/report_schema.json
tests/
```
