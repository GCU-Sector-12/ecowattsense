# EcoWattSense API contract

Version 0.1 (draft for team review). Date: 19 October 2026.
Author: Dariusz Jaroch.
Related YouTrack tasks: EWS_API-1. Source: database brief (docs/design/database-data-brief.md, KB EWS-A-12), implementation plan (docs/design/implementation-plan.md).

## 1. What this document is

This document is the agreement on names between the client, the server and the dashboard. It lists every endpoint with the data that goes in and the data that comes back. Every backend task waits for it. Where an older text uses another name, the name in this document wins. The older texts are listed in section 8.

## 2. Basics

- The base path is `/api`.
- Requests and replies are JSON. The header is `Content-Type: application/json`.
- Times are ISO 8601 in UTC with a `Z` at the end, for example `2026-10-19T14:05:00Z`. The server stores UTC.
- All names are `snake_case`. The unit is the last part of the name (`_seconds`, `_uj`, `_watts`). A moment saved by the server ends with `_at`. A name is the same in JSON, in the database and in the code.
- `device_id` is a UUID. The client makes it at first start. It never changes.
- There are two kinds of callers. The client runs on a laptop. The admin uses the dashboard.

Errors have three shapes:

| Status | Body | When |
|---|---|---|
| 401 | `{"detail": "..."}` | The key, the password or the session is wrong or missing |
| 422 | The FastAPI validation body: `detail` is a list, each item has `loc`, `msg` and `type` | The request is not valid |
| 5xx | Any | The server has a problem. The client tries again later |

## 3. Who may call what

There are two mechanisms.

**Client key.** The client sends the header `X-EWS-Key: <key>`. The value is the setting `EWS_CLIENT_KEY` in the server `.env` file. One key is used by all clients. A wrong or missing key gives 401. Only `POST /api/reports` uses the client key.

**Admin session.** `POST /api/login` sets a cookie named `ews_session`. The cookie is `HttpOnly` and `SameSite=Lax`. It is `Secure` when the server is reached over HTTPS. Every GET for the dashboard needs the cookie. A missing or expired cookie gives 401. The session is used by `/api/devices`, `/api/devices/{device_id}/history`, `/api/summary` and `/api/logout`.

Names of settings in the `.env` file:

| Name | Status | Meaning |
|---|---|---|
| `EWS_CLIENT_KEY` | agreed | The client key for `POST /api/reports` |
| `EWS_ADMIN_USERNAME` | proposed | The user name of the one admin |
| `EWS_ADMIN_PASSWORD` | proposed | The password of the admin. Only in `.env`, never in the repository |

The other server settings keep their current names for now. Whether they also get the prefix `EWS_` is an open point (see section 9).

## 4. Endpoints at a glance

| Method | Path | Caller | Auth | Purpose |
|---|---|---|---|---|
| GET | `/api/health` | anyone | none | Check that the server is running |
| POST | `/api/login` | admin | none | Start an admin session |
| POST | `/api/logout` | admin | cookie | End the session |
| POST | `/api/reports` | client | `X-EWS-Key` | Send one report or a list of reports |
| GET | `/api/devices` | admin | cookie | List the computers with their state now |
| GET | `/api/devices/{device_id}/history` | admin | cookie | Reports and totals of one computer |
| GET | `/api/summary` | admin | cookie | Counts, energy, cost and CO2 for one day |

## 5. Endpoints

The examples use one computer: `device_id` `6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a12`, `display_name` `Lab 1 PC 03`, location `Lab 1` in the building `George Moore`, `os` `windows`, `device_type` `laptop`. The date is 19 October 2026.

### 5.1 GET /api/health

- Caller: anyone. Auth: none.
- Response 200: the field `status`, always `ok`.

Status codes: 200.

Example request: `GET /api/health`

Example response:

```json
{"status": "ok"}
```

### 5.2 POST /api/login

- Caller: the admin. Auth: none, the password is the check.
- There is one admin user. The user name and the password come from `EWS_ADMIN_USERNAME` and `EWS_ADMIN_PASSWORD` (section 3). The password in `.env` is plain text. The server compares it and never writes it anywhere. The table `users` with password hashes stays for later (section 9).

Request body:

| Field | Type | Required | Meaning |
|---|---|---|---|
| `username` | string | yes | The admin user name |
| `password` | string | yes | The admin password |

Response 200 (and the cookie `ews_session`):

| Field | Type | Meaning |
|---|---|---|
| `username` | string | The user who logged in |

Status codes:

- 200: Logged in. The cookie is set
- 401: `{"detail": "Wrong username or password"}`
- 422: A field is missing

Example request:

```json
{"username": "admin", "password": "change-me"}
```

Example response:

```json
{"username": "admin"}
```

### 5.3 POST /api/logout

- Caller: the admin.
- Auth: the cookie `ews_session`.
- Request: no body. Response: 204 with no body. The cookie is cleared.

Status codes:

- 204: Logged out
- 401: There is no valid cookie

### 5.4 POST /api/reports

- Caller: the client.
- Auth: the header `X-EWS-Key`.
- Request body: one report object, or a list of 1 to 100 report objects. A client that was offline sends what it queued, oldest first.

The fields come from the table `reports` in the brief (section 5.3) plus two device fields.

| Field | Type | Required | Meaning |
|---|---|---|---|
| `device_id` | string, uuid | yes | The computer. A random UUID made by the client at first start. It never changes |
| `timestamp` | string, date-time UTC | yes | The end of the interval this report covers. A time without a zone is rejected. A time more than 5 minutes ahead of the server clock is rejected |
| `interval_seconds` | integer, more than 0 | yes | The seconds this report covers. It can be shorter than the setting (sleep, shutdown) |
| `state` | integer, 2, 1 or 0 | yes | 2 active, 1 idle, 0 unused |
| `idle_seconds` | integer, 0 or more | yes | Time without user input at the end of the interval |
| `cpu_percent` | number, 0 to 100 | yes | Average CPU load in the interval |
| `screen_locked` | boolean | yes | The screen is locked at the end of the interval |
| `os` | string | yes | `windows`, `linux` or `macos`. Used when the first report creates the device |
| `device_type` | string | yes | Pattern `^[a-z][a-z0-9_]*$`, at most 40 characters, for example `laptop` or `desktop`. It picks the row in `energy_rates` |
| `energy_uj` | integer, 0 or more, or null | no | Energy taken from the wall socket in the interval, in microjoules (section 6) |

Unknown fields are rejected with 422. When `energy_uj` is left out or null, the server estimates it. It takes the watts for the state from `energy_rates`, and multiplies by `interval_seconds` and by 1 000 000. It stores `energy_source` `estimated`. When `energy_uj` is sent, the server stores it as it is, with `energy_source` `measured`. A laptop on its battery sends 0, not nothing.

Rules:

- The first report for a new `device_id` creates the device. `display_name` and the location are null. `first_seen_at` is `received_at`. There is no register step and no client login.
- Every accepted report updates `last_seen_at`.
- A report with the same `device_id` and `timestamp` as a stored one is a duplicate. It is not stored again. It is not an error.
- A report with a `timestamp` older than the newest stored one is still stored (a late report).

Response body, for one report and for a list:

| Field | Type | Meaning |
|---|---|---|
| `results` | list | One item for each report, in the same order |
| `results[].index` | integer | The place of the report in the request. 0 for a single report |
| `results[].status` | string | `created`, `duplicate` or `rejected` |
| `results[].error` | string or null | The first problem, in words. A client must not parse it |
| `settings` | object | The four settings of section 7 |

The values of `status`:

| Value | Meaning | What the client does |
|---|---|---|
| `created` | The report is stored | Marks it as sent |
| `duplicate` | The server has it already | Marks it as sent |
| `rejected` | This one report is wrong. A retry will not help | Marks it and does not send it again |

Status codes:

- 201: A single report was stored
- 200: A single report was a duplicate, or the body was a list. For a list, 200 is always the answer, with one result for each report
- 401: The key is wrong or missing. Nothing is stored
- 422: A single report is wrong (FastAPI validation body, not a results list). Also: the list has 0 or more than 100 items, or the body is not JSON (`{"detail": "..."}`)

The client reads `settings` from every reply and applies them from the next interval.

Example request, one report from a laptop on the grid, with no `energy_uj`:

```json
{"device_id": "6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a12", "timestamp": "2026-10-19T14:05:00Z", "interval_seconds": 300, "state": 2, "idle_seconds": 12, "cpu_percent": 37.5, "screen_locked": false, "os": "windows", "device_type": "laptop"}
```

Example response, status 201:

```json
{
  "results": [
    {"index": 0, "status": "created", "error": null}
  ],
  "settings": {"report_interval_seconds": 300, "idle_threshold_seconds": 600, "unused_threshold_seconds": 3600, "no_data_after_cycles": 3}
}
```

Example request, a list of two reports. The second one was sent before and is a duplicate:

```json
[
  {"device_id": "6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a12", "timestamp": "2026-10-19T14:10:00Z", "interval_seconds": 300, "state": 1, "idle_seconds": 640, "cpu_percent": 4.2, "screen_locked": false, "os": "windows", "device_type": "laptop", "energy_uj": 4500000000},
  {"device_id": "6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a12", "timestamp": "2026-10-19T14:05:00Z", "interval_seconds": 300, "state": 2, "idle_seconds": 12, "cpu_percent": 37.5, "screen_locked": false, "os": "windows", "device_type": "laptop"}
]
```

Example response, status 200:

```json
{
  "results": [
    {"index": 0, "status": "created", "error": null},
    {"index": 1, "status": "duplicate", "error": null}
  ],
  "settings": {"report_interval_seconds": 300, "idle_threshold_seconds": 600, "unused_threshold_seconds": 3600, "no_data_after_cycles": 3}
}
```

### 5.5 GET /api/devices

- Caller: the admin, from the dashboard.
- Auth: the cookie `ews_session`.

Query parameters:

| Name | Type | Required | Meaning |
|---|---|---|---|
| `location_id` | integer | no | Only the computers in this room |
| `include_archived` | boolean | no | Default false. When true, archived computers are listed too |

Response 200:

| Field | Type | Meaning |
|---|---|---|
| `devices` | list | The computers, sorted by `display_name`, then `device_id` |
| `devices[].device_id` | string, uuid | The computer |
| `devices[].display_name` | string or null | The name set by the admin |
| `devices[].location_id` | integer or null | The room |
| `devices[].location_name` | string or null | The name of the room |
| `devices[].building` | string or null | The building of the room |
| `devices[].os` | string | `windows`, `linux` or `macos` |
| `devices[].device_type` | string | For example `laptop` |
| `devices[].state` | integer or null | The state of the newest report. Null when the device has no data |
| `devices[].state_name` | string | `active`, `idle`, `unused` or `no_data` |
| `devices[].last_seen_at` | string, date-time UTC | The time of the last report |
| `devices[].first_seen_at` | string, date-time UTC | The time of the first report |
| `devices[].archived` | boolean | True when the admin archived the computer |

`display_name`, `location_id`, `location_name` and `building` are null until an admin sets them.

`state_name` is `no_data` when there was no report for `no_data_after_cycles` times `report_interval_seconds` seconds. With the defaults this is 3 times 300 seconds, so 15 minutes. The dashboard shows the word, never only the colour.

Status codes:

- 200: The list, also when it is empty
- 401: There is no valid cookie
- 422: A query parameter has the wrong type

Example request: `GET /api/devices?location_id=1&include_archived=false`

Example response:

```json
{
  "devices": [
    {"device_id": "6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a12", "display_name": "Lab 1 PC 03", "location_id": 1, "location_name": "Lab 1", "building": "George Moore", "os": "windows", "device_type": "laptop", "state": 2, "state_name": "active", "last_seen_at": "2026-10-19T14:05:00Z", "first_seen_at": "2026-10-12T09:00:00Z", "archived": false}
  ]
}
```

### 5.6 GET /api/devices/{device_id}/history

- Caller: the admin, from the dashboard.
- Auth: the cookie `ews_session`.

Path and query parameters:

| Name | Type | Required | Meaning |
|---|---|---|---|
| `device_id` | string, uuid | yes | The computer (in the path) |
| `from` | string, date-time UTC | no | Start of the range. Default: 24 hours before `to` |
| `to` | string, date-time UTC | no | End of the range. It is exclusive. Default: now |

The range is at most 31 days. A longer range gives 422.

Response 200:

| Field | Type | Meaning |
|---|---|---|
| `device_id` | string, uuid | The computer |
| `display_name` | string or null | The name set by the admin |
| `from` | string, date-time UTC | The start of the range used |
| `to` | string, date-time UTC | The end of the range used |
| `reports` | list | The reports in the range, oldest first |
| `totals` | object | Sums for the range |

Fields of each item in `reports`: `timestamp`, `interval_seconds`, `state`, `idle_seconds`, `cpu_percent`, `screen_locked`, `energy_uj` and `energy_source`. They have the meaning given in section 5.4. `energy_source` is `estimated` or `measured`.

Fields of `totals`:

| Field | Type | Meaning |
|---|---|---|
| `active_time_seconds` | integer | Sum of `interval_seconds` of the reports in state 2 |
| `idle_time_seconds` | integer | Sum of `interval_seconds` of the reports in state 1 |
| `unused_time_seconds` | integer | Sum of `interval_seconds` of the reports in state 0 |
| `energy_uj` | integer | Sum of `energy_uj` |
| `cost_gbp` | number | Cost in pounds |
| `co2_kg` | number | CO2 in kilograms |

The time in a state is the sum of `interval_seconds`, not the number of reports times the interval. This is the rule of the brief. The totals end with `_time_seconds`, because `idle_seconds` in a report means something else (time without input). Cost and CO2 use the tariff that was valid at the `timestamp` of each report.

Status codes:

- 200: The history, also when there are no reports in the range
- 401: There is no valid cookie
- 404: `{"detail": "Device not found"}` for an unknown `device_id`
- 422: A parameter has the wrong type, or the range is longer than 31 days

Example request: `GET /api/devices/6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a12/history?from=2026-10-18T14:00:00Z&to=2026-10-19T14:00:00Z`

Example response:

```json
{
  "device_id": "6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a12",
  "display_name": "Lab 1 PC 03",
  "from": "2026-10-18T14:00:00Z",
  "to": "2026-10-19T14:00:00Z",
  "reports": [
    {"timestamp": "2026-10-19T14:05:00Z", "interval_seconds": 300, "state": 2, "idle_seconds": 12, "cpu_percent": 37.5, "screen_locked": false, "energy_uj": 9000000000, "energy_source": "estimated"}
  ],
  "totals": {"active_time_seconds": 18000, "idle_time_seconds": 3600, "unused_time_seconds": 600, "energy_uj": 123456789000, "cost_gbp": 0.0086, "co2_kg": 0.0071}
}
```

### 5.7 GET /api/summary

- Caller: the admin. Auth: the cookie `ews_session`.

Query parameters:

| Name | Type | Required | Meaning |
|---|---|---|---|
| `date` | string, `YYYY-MM-DD` | no | The day. Default: today in UK local time. The day runs in UK local time, as the brief says |

Response 200:

| Field | Type | Meaning |
|---|---|---|
| `date` | string | The day |
| `counts` | object | Computers per state: `active`, `idle`, `unused`, `no_data` and `total` |
| `energy_uj` | integer | Energy of all computers for the date |
| `cost_gbp` | number | Cost of all computers for the date |
| `co2_kg` | number | CO2 of all computers for the date |
| `price_gbp_per_kwh` | number | The price valid now |
| `co2_kg_per_kwh` | number | The CO2 factor valid now |
| `series` | list | The energy in steps of 30 minutes |

`counts` shows the state now (the newest report of each computer). It is not the state on the date.

`series` has one point for each 30 minutes of the date, so 48 points (46 or 50 on the days the clocks change). A bucket that has not happened yet is left out. `timestamp` is the start of the bucket, in UTC. In UK summer time the first bucket of the day starts at 23:00 UTC of the day before. Each point has `timestamp` and `energy_uj`.

The dashboard converts the units. 1 kWh is 3 600 000 000 000 µJ (3.6e12). The average power in a 30-minute bucket in kW is `energy_uj` divided by 1.8e12.

Status codes:

- 200: The summary
- 401: There is no valid cookie
- 422: `date` is not a date

Example request: `GET /api/summary?date=2026-10-19`

Example response:

```json
{
  "date": "2026-10-19",
  "counts": {"active": 12, "idle": 5, "unused": 2, "no_data": 1, "total": 20},
  "energy_uj": 48600000000000,
  "cost_gbp": 3.38,
  "co2_kg": 2.79,
  "price_gbp_per_kwh": 0.25,
  "co2_kg_per_kwh": 0.207,
  "series": [
    {"timestamp": "2026-10-18T23:00:00Z", "energy_uj": 900000000000},
    {"timestamp": "2026-10-18T23:30:00Z", "energy_uj": 950000000000}
  ]
}
```

## 6. Values

**States.** The code is stored in the database and sent in reports. The name is used in replies and on screen.

| Code | Name | Meaning |
|---|---|---|
| 2 | active | A person is using the computer |
| 1 | idle | No input for the idle threshold |
| 0 | unused | No input for the unused threshold |
| none | no_data | Reports stopped. Only in replies, never stored |

**Energy rule.** `energy_uj` is the energy taken from the wall socket. A laptop on its battery reports 0. This is the number that is summed, billed and used for CO2. A laptop on its battery still sends a report, with an explicit 0. If the field were missing, the server would estimate the energy as if the laptop were on the grid.

**Other values.**

| Item | Rule |
|---|---|
| Energy | Whole number of microjoules (`_uj`). It needs a 64-bit integer |
| Time spans | Whole seconds (`_seconds`) |
| Money | Pounds (`_gbp`) |
| CO2 | Kilograms (`_kg`) |
| UUID and time | UUID in lower case with hyphens. Time in ISO 8601 UTC with `Z` |
| `energy_source` | `estimated` or `measured` |

## 7. Settings the server sends to the client

The server sends four settings in every reply to `POST /api/reports`. The names are the keys in the brief (section 5.6).

| Name | Default | Meaning |
|---|---|---|
| `report_interval_seconds` | 300 | How often the client sends a report |
| `idle_threshold_seconds` | 600 | Seconds without input before the state is idle |
| `unused_threshold_seconds` | 3600 | Seconds without input before the state is unused |
| `no_data_after_cycles` | 3 | Missing report cycles before a computer shows as no data |

The admin changes the settings in the dashboard. The endpoints for that are not in this version of the document.

## 8. What this document settles

1. **The energy rule.** `energy_uj` is the energy from the wall socket. A laptop on its battery reports 0. See section 6.
2. **The settings in the reply.** The server sends the four names of brief section 5.6 after every report. See section 7.
3. **The name of the client key.** The header is `X-EWS-Key`. The setting in `.env` is `EWS_CLIENT_KEY`. The names `CLIENT_SECRET`, `X-Client-Secret` and `ECOWATTSENSE_API_KEY` are dropped.
4. **A new computer.** The first report creates it. The register step and the client login are gone (decision of 11 October). The state names are Active, Idle and Unused, with the codes 2, 1 and 0. In JSON the names are lower case.

What changes elsewhere to match this document:

- ADR-001, the example report in section 6.
- KB EWS-A-17, section 11.
- KB EWS-A-15, the parts about registration and client login.
- `docs/design/dashboard-sample-data.json`, which gets the same names as this document. Its fields `last_seen_text`, `estimated_watts` and `summary.display` are for the screen only. They are not part of the API.
- `server/ecowattsense_server/config.py` and `.env.example`. The setting `ECOWATTSENSE_API_KEY` becomes `EWS_CLIENT_KEY`.

## 9. Open points

- The prefix of the other server settings. Today it is `ECOWATTSENSE_`. It could be `EWS_`.
- Two optional measured fields, `battery_energy_uj` and `device_energy_uj`, that a client with a battery sensor could send. They are not in the brief and are not stored. Today they are rejected as unknown fields.
- Endpoints for settings, energy rates, tariffs, locations and device rename (plan cards SRV-10 and SRV-11). They come in the next version.
- A `settings_version` field in the reply, so a client can see that the settings changed.
- The lifetime of the session cookie.
- Admin users in the `users` table with password hashes, instead of one user in `.env`.
- How `series` looks for a day with no reports.
