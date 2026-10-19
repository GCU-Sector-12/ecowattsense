# Shared

Things the client, the server and the dashboard must agree on.

## report_schema.json

The JSON Schema of one report. The client builds its reports from it. The server checks every report against it. The dashboard uses the same names. A change here is its own pull request, and both sides change together.

Fields, required first:

| Name | Required | Meaning |
|---|---|---|
| `device_id` | yes | The computer. A random UUID made at the first start. |
| `timestamp` | yes | End of the interval, in UTC with a zone. |
| `interval_seconds` | yes | Seconds the report covers. |
| `state` | yes | 2 active, 1 idle, 0 unused. |
| `idle_seconds` | yes | Seconds since the last keyboard or mouse input. |
| `cpu_percent` | yes | Processor use in the interval, 0 to 100. |
| `screen_locked` | yes | True when the screen is locked. |
| `on_battery` | yes | True when a laptop runs on its battery. |
| `os` | yes | `windows`, `linux` or `macos`. |
| `device_type` | yes | For example `desktop` or `laptop`. |
| `energy_uj` | no | Energy from the wall socket, in microjoules. |
| `battery_energy_uj` | no | Energy into the battery (above 0) or out of it (below 0). |
| `device_energy_uj` | no | Energy the computer used, from grid or battery. |
| `machine_hash` | no | SHA-256 of a hardware id, in lower case hex. |

## Energy rule

`energy_uj` is the energy taken from the wall socket, so a laptop on its battery sends `on_battery` true and `energy_uj` 0. When `energy_uj` is missing and `on_battery` is true, the server stores 0 and does not estimate.

## State names

Active, Idle and Unused, with codes 2, 1 and 0.

## examples/

Each file is one report and must pass the schema.

- `active.json`: a desktop in use, with no energy fields.
- `idle.json`: a laptop idle on its battery.
- `unused.json`: a laptop that is unused and charging.

## More

- `docs/design/database-data-brief.md`
- `docs/design/api-contract.md`
