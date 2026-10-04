# Data we store: what the ERD needs (input for EWS-28)

**For:** Jason (EWS-28, database design and ER diagram). **From:** Dariusz. **Status:** draft v0.7, 4 October 2026.

This page says what data we keep, the rules for it, the naming rules and the list of tables and columns. The ERD itself is your work: the diagram, the notation and the details of the relationships. The names are fixed here, so the database, the API and the code use the same names.

## 1. What the database is for

The server keeps all data in one SQLite database (we use SQLModel, so PostgreSQL is possible later). The dashboard reads everything from it. The admin sees which computers are active, idle or unused, the history, and the energy, cost and CO2.

## 2. What we store

**Computers**
- Each monitored computer has an ID. The client makes it at first start (a random UUID). It never changes.
- We know the operating system (windows, linux or macos), the type (at the start desktop or laptop, more types can come later), the time of the first report and the time of the last report.
- The admin can give a computer a name (for example "Lab 1 PC 03") and put it in a room.
- The admin can archive an old computer. It is hidden in the list, but its history stays.
- Optional, later: a hash of a hardware ID. It helps to find the same computer after a reinstall. It is a hint, not a key, because it is not always unique.

**Rooms**
- Name (for example "Lab 1") and building (optional).
- A room has many computers. A computer is in one room or in no room.

**Reports**
- A client sends one report per report interval. The admin sets the interval (default 300 seconds).
- The client sends: computer ID, time of the measurement, interval in seconds, state, idle seconds, CPU % and screen locked (yes or no).
- If the client can measure real energy, it also sends the measured energy for the interval (in µJ).
- The server adds: the time when it got the report, the energy for the interval (the measured value, or an estimate when there is no measurement) and where the energy came from (estimate or measurement).
- Only one report per computer and time. After a network problem the client can send the same report again, so the database must not save it twice.
- We keep all reports (full history). With the default interval this is up to 288 reports a day per computer. 8 laptops give about 70,000 rows a month.

**State codes**
- 2 = active, 1 = idle, 0 = unused. Stored as a small number, the same as in the report JSON. A higher number means more activity.
- "No data" is not stored. The server works it out when reports stop.

**Watts per device type and state (for the estimate)**
- One row for each device type. Each row has 3 values, one for each state (active, idle, unused). The admin can change them.
- At the start we have 2 device types (desktop and laptop), so there are 2 rows. This is not a fixed number. In real use there will be more device types (for example all-in-one, workstation or thin client). Each new type adds 1 row.
- We use them only when we have no real reading. Then the server estimates the energy when it saves a report: energy in µJ = watts × interval in seconds × 1,000,000.
- Real readings come later (hardware energy counters or the battery, EWS-A-10 NV-04). A client that can measure sends the energy itself, and the server saves it as it is.
- A change of watts does not change past energy, because the energy is already saved in each report.

**Prices (tariffs)**
- Price per kWh (GBP) and CO2 per kWh, each row with a "valid from" time.
- A row is valid until the next row starts. Old rows stay.
- Cost and CO2 of a report use the row that was valid at the time of the report. So a new price does not change past cost.

**Settings**
- Report interval, idle threshold (default 10 minutes), unused threshold (default 60 minutes), and the number of missing reports before "No data" (default 3). All times in seconds.

**Admin accounts**
- User name and password hash (never the plain password). Admin only, no roles in the prototype.

## 3. Rules for all data

- **No personal data.** No user names, host names, IP or MAC addresses, window titles or keystrokes. Computers are not linked to people.
- **Energy in µJ** (microjoules) as a whole number. One report can be bigger than 32 bits (30 W for 300 s = 9,000,000,000 µJ), so it needs a 64-bit integer. We convert to Wh or kWh only for display.
- **Times in UTC.** Daily totals use UK local time.
- **Time in a state** = the sum of the interval seconds, not the number of reports × interval, because the admin can change the interval.

## 4. Naming rules

Use these rules for every table and column. The report JSON and the Python code use the same style, so a name is written one way everywhere.

1. **Style.** `snake_case`, lower case, English, no spaces, no Polish letters. Example: `idle_seconds`. PostgreSQL turns a name without quotes into lower case, so a name like `createdAt` would need quotes after the move from SQLite.
2. **Tables.** Plural nouns: `devices`, `reports`. `user` is a reserved word in PostgreSQL, so the table is `users`.
3. **Keys.** The primary key is always `id`. A foreign key is the singular table name plus `_id`: `device_id` points to `devices.id`. The report JSON uses the same name.
4. **Columns.** A singular noun, without the table name in front (`reports.state`, not `reports.report_state`). A name from the report JSON stays as it is, also when it starts with the table name (`devices.device_type`). Yes or no columns have no `is_` in front (`archived`, `screen_locked`).
5. **Units.** The unit is the last part of the name: `interval_seconds`, `cpu_percent`, `active_watts`, `energy_uj` (microjoules), `price_gbp_per_kwh`.
6. **Time.** A moment saved by the server ends with `_at` (`received_at`). The start of a validity period is `valid_from`. The one exception is `timestamp`, the time of the measurement, which keeps the name from the report JSON.

If you need a name that these rules do not cover, use the same style and tell Dariusz.

## 5. Tables and columns

This is the list for the ERD. The types are general types for the diagram. SQLite stores `uuid` and `datetime` as text and `boolean` as 0 or 1. All times are in UTC.

### 5.1 `locations` (rooms)

| Column | Type | Rules | Meaning |
|---|---|---|---|
| `id` | integer | primary key | |
| `name` | text | required, unique | For example "Lab 1" |
| `building` | text | optional | |

### 5.2 `devices` (monitored computers)

| Column | Type | Rules | Meaning |
|---|---|---|---|
| `id` | uuid | primary key | Random, made by the client at first start. It never changes. It is `device_id` in the report JSON |
| `display_name` | text | optional | Set by the admin, for example "Lab 1 PC 03" |
| `location_id` | integer | foreign key to `locations.id`, optional | Empty means no room |
| `machine_hash` | text | optional | Hash of a hardware ID. A hint, not unique |
| `os` | text | required | `windows`, `linux` or `macos` |
| `device_type` | text | required | The type of computer, for example `desktop` or `laptop`. More types can come later (see 5.4) |
| `first_seen_at` | datetime | required | Time of the first report |
| `last_seen_at` | datetime | required | Time of the last report |
| `archived` | boolean | required, default false | Hidden in the list. The history stays |

### 5.3 `reports` (one row per report from a client)

| Column | Type | Rules | Meaning |
|---|---|---|---|
| `id` | integer | primary key | |
| `device_id` | uuid | foreign key to `devices.id`, required | |
| `timestamp` | datetime | required | Time of the measurement, from the client |
| `received_at` | datetime | required | Time when the server got the report |
| `interval_seconds` | integer | required, more than 0 | The report interval at that time. The admin sets it |
| `state` | smallint | required, 2, 1 or 0 | 2 = active, 1 = idle, 0 = unused. A small number (one byte) |
| `idle_seconds` | integer | required, 0 or more | |
| `cpu_percent` | float | required, 0 to 100 | |
| `screen_locked` | boolean | required | |
| `energy_uj` | bigint | required, 0 or more | Energy for the interval in µJ. From the client if it measured it, otherwise calculated by the server |
| `energy_source` | text | required | `estimated` or `measured` |

Unique: `device_id` and `timestamp` together, so the same report is not saved twice. Index on `timestamp`.

### 5.4 `energy_rates` (watts for the estimate)

| Column | Type | Rules | Meaning |
|---|---|---|---|
| `id` | integer | primary key | |
| `device_type` | text | required, unique | The type of computer, for example `desktop` or `laptop`. The same values as in `devices.device_type` |
| `active_watts` | float | required, 0 or more | Estimated power in state 2 (active) |
| `idle_watts` | float | required, 0 or more | Estimated power in state 1 (idle) |
| `unused_watts` | float | required, 0 or more | Estimated power in state 0 (unused) |

One row for each device type. The admin can change the watts. At the start we have 2 rows: `desktop` and `laptop`. This is not a fixed number. In real use there will be more device types, for example `all_in_one`, `workstation` or `thin_client`. Each new type adds 1 row. So please do not limit `device_type` to 2 values in the ERD. Each row has one watts column for each state, so the table stays flat and easy to read. The 3 states are fixed. The server picks the column from the state of the report: 2 is `active_watts`, 1 is `idle_watts` and 0 is `unused_watts`.

### 5.5 `tariffs` (price and CO2 over time)

| Column | Type | Rules | Meaning |
|---|---|---|---|
| `id` | integer | primary key | |
| `valid_from` | datetime | required, unique | A row is valid until the next row starts. The first row starts at 2000-01-01 |
| `price_gbp_per_kwh` | float | required, 0 or more | |
| `co2_kg_per_kwh` | float | required, 0 or more | |

### 5.6 `settings`

| Column | Type | Rules | Meaning |
|---|---|---|---|
| `id` | integer | primary key | |
| `key` | text | required, unique | Name of the setting |
| `value` | text | required | Value of the setting |

Keys and default values: `report_interval_seconds` (300), `idle_threshold_seconds` (600), `unused_threshold_seconds` (3600), `no_data_after_cycles` (3).

### 5.7 `users` (admin accounts)

| Column | Type | Rules | Meaning |
|---|---|---|---|
| `id` | integer | primary key | |
| `username` | text | required, unique | |
| `password_hash` | text | required | Never the plain password |
| `created_at` | datetime | required | |

### 5.8 Relationships

- One room has many computers: `devices.location_id` points to `locations.id`. A computer can be in no room.
- One computer has many reports: `reports.device_id` points to `devices.id`. A report always has a computer.
- `energy_rates`, `tariffs`, `settings` and `users` have no foreign keys. The values in `energy_rates.device_type` match `devices.device_type`, and the three watts columns match the state codes in `reports.state`, but `energy_rates` is a lookup table, not a link.
- Our suggestion for deleting: if a room is deleted, its computers stay without a room. A computer with reports is not deleted. The admin archives it. You can choose other rules.

## 6. Not in the database

- Calculated when needed: the state now, "No data", cost, CO2 and daily totals.
- Files on each computer: settings, the buffer of unsent reports and logs. They are not part of the ERD.
- Next version, not in the prototype (EWS-A-10): alerts, day and night prices, daily summaries, more user roles.

## 7. Your survey in short

1. Users: computers are not linked to people.
2. Data from the client: see "Reports". No hostname, RAM or network data. In the prototype the server estimates the energy. If the client can measure it later, the client sends it.
3. Authentication: admin login only.
4. Rooms: yes.
5. Alerts: not in the prototype.
6. History: full history.

## 8. Your decisions

The diagram notation, the details of the relationships (for example what happens when something is deleted) and any extra tables or columns you need. The names and columns in Sections 4 and 5 are agreed. If a name, a column or a rule in Sections 2 to 5 does not work for the ERD, please tell Dariusz before you change it.

---
*Written with AI help (Claude) from the team's design notes and your survey. Decisions: Dariusz Jaroch.*
*v0.7, 4 October: the example report is removed. v0.6: `energy_rates` is a flat table with one row for each device type and one watts column for each state. v0.4: more device types are possible. v0.3: naming rules and the list of tables and columns. v0.2: watts are only for the estimate. A report can carry measured energy, and it keeps where its energy came from.*
