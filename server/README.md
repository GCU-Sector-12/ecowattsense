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
