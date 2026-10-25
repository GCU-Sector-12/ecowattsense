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

## Run it

One script does the setup, on Windows, macOS and Linux. It creates `server/.venv` on the first run and installs `requirements.txt` into it. You need Python 3.11 or newer.

```bash
python scripts/server.py run      # server on http://localhost:8000, reloads on file changes
python scripts/server.py test     # pytest in server/tests
```

On macOS and Linux use `python3` instead of `python`. Extra arguments go to the tool, for example `python scripts/server.py test -v` or `python scripts/server.py run --port 8080`.

Check that it is up: open http://localhost:8000/api/health, you should see `{"status": "ok"}`.

By hand, without the script:

```bash
cd server
python -m venv .venv
.venv\Scripts\activate          # Windows;  macOS and Linux: source .venv/bin/activate
pip install -r requirements.txt
uvicorn ecowattsense_server.main:app --reload
pytest
```

Settings come from `.env` in the repository root, see `.env.example`.

## API docs and API contract

The docs of the API that is implemented are at http://localhost:8000/api/docs.
The API contract is at http://localhost:8000/api/contract, as a Swagger UI page. The raw file is at `/api/contract/openapi.yaml` and as JSON at `/api/contract/openapi.json`.
The server reads the contract from `shared/openapi-contract.yaml`. To use another file, set `ECOWATTSENSE_CONTRACT_PATH`.
All of these are under `/api`, so they also work on staging through the proxy.

## Admin sessions (EWS_API-10)

Set `EWS_ADMIN_USERNAME` and `EWS_ADMIN_PASSWORD` in the root `.env` before
logging in. Empty credentials never permit login. POST JSON to `/api/login`;
reuse its `ews_session` cookie for dashboard requests and `/api/logout`.
Sessions expire after eight hours and logout revokes the token on the server.
Cookies are HttpOnly, SameSite=Lax, and Secure on HTTPS. The existing Uvicorn
proxy configuration supplies the HTTPS scheme behind the staging proxy.

The W6 session store is in memory: use one server worker. A restart logs everyone
out. Multiple workers or replicas need a shared session store first.

## Client stubs and dashboard reads (EWS_API-8/11/12, EWS-35)

`GET /api/settings`, `POST /api/ping` and `PUT /api/devices/{device_id}` are W6
stubs protected by `X-EWS-Key`. They do not store anything: ping always returns
`registered: true`, and registration always returns `created: true`.

The dashboard routes require an admin session:

- `GET /api/devices?location_id=1&include_archived=false`
- `GET /api/devices/{device_id}/history?from=2026-10-19T13:00:00Z&to=2026-10-19T15:00:00Z`
- `GET /api/summary?date=2026-10-19`

Default `ECOWATTSENSE_DASHBOARD_DATA_SOURCE=stub` serves three sample computers,
fixed sample summary buckets and sample history. Use device ID
`6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a12` with the history range above. Those
reports cross a sample tariff change at 14:00, so earlier energy keeps its old
price. Sample registration does not change the dashboard fixture.

The W6 summary deliberately fills the whole selected day, as API-12 requests.
It has 48 half-hour buckets normally, or 46/50 on UK clock-change days, following
the contract's UK-day rule. Dates and UTC timestamps are deterministic for the
requested day. This is sample data, not collected measurements.

Set `ECOWATTSENSE_DASHBOARD_DATA_SOURCE=database` after the database team's
`devices`, `reports`, `locations` and `tariffs` tables and tariff seed exist.
The readers reflect the actual tables; this change does not create tables,
change models, seed data or implement report ingestion. Missing tables or
missing tariff coverage return 503 instead of silently displaying sample data.

Database mode reads the newest report for current states, hides archived
computers from the default list and current counts, and preserves their history.
History uses an inclusive start and exclusive end, defaults to the last 24 hours,
and accepts positive ranges up to 31 days. State durations sum actual report
intervals. Energy totals use wall energy only. Cost and CO2 apply the tariff at
each report timestamp, including the exact tariff boundary. Summary accepts a
UK-local date, covers all reports for that day, and omits future buckets.

Validation: `python scripts/server.py test`. Database integration tests use
isolated in-memory SQLite tables matching the data brief; they never touch the
configured application database. Deployment and a staging smoke test still
require reviewed PRs and the database prerequisites. PR CI belongs to EWS_API-4;
this repository currently has no server-test PR workflow.
