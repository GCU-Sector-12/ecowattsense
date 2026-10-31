# Local test mock — not the real backend

Python 3.9+ standard library only. No packages, backend, staging or real credentials.
Only 127.0.0.1 is bound; the default port is 8765. The fixed dummy key is
`local-test-key`. Supplied authentication keys are never logged.

Open two PowerShell terminals in Rider, both in `client/windows/EcoWattSense.Client`.

Terminal 1:

```powershell
python .\tests\mock_server.py
```

Use `py -3` or the full path to an existing Python executable if `python` is unavailable.
Optional port override: `python .\tests\mock_server.py --port 8766`.

Terminal 2 (the following key is deliberately public dummy test data):

```powershell
$env:Server__ClientKey = 'local-test-key'
dotnet run -- --Server:Enabled=true --Server:BaseUrl=http://127.0.0.1:8765 --Reporting:DeviceType=laptop --DeviceIdentity:Directory="$env:TEMP\EcoWattSense-ManualIdentityTest"
```

The URL and enabled flag are command-line overrides so an existing environment setting
cannot redirect this command to staging. Do not substitute a real key. Settings from the
mock should produce a startup log with source `server`, report interval 15, idle 10 and
unused 20 seconds. The identity log should load the existing test UUID
`8f20c31c-77ed-446d-a681-7717e3f62c25`. No machine-specific path is saved in configuration.
Expect the first report about 15 seconds after the CPU baseline, provided required readings
are available. Terminal 2 logs acceptance; terminal 1 prints each newly accepted report as JSON.

Stop each terminal process with Ctrl+C. After stopping the client, clear the dummy environment
variable with `Remove-Item Env:Server__ClientKey`. Close the test terminal to discard other
session environment changes. Never reuse this setup for a real server.

All accepted reports and duplicate tracking are in memory. **Data disappears when the mock
stops.** The persistent client UUID remains in its existing temporary identity directory.

Scope: GET /api/settings and POST /api/reports only. The mock accepts one object with exactly
the ten fields emitted by this client. It rejects lists, unknown fields, optional energy/hash
fields, duplicate JSON properties, invalid JSON, and bodies over 16 KiB with 422. It validates
UUID, UTC-Z timestamp (including the contract's five-minute future limit), integer durations
and state without accepting booleans, finite CPU 0–100, booleans, OS enum, and device-type pattern.
A desktop cannot claim on_battery true. No database, energy-rate lookup, or other backend
business logic is simulated. A valid report returns 201/created, and a repeated UUID/time
returns 200/duplicate. Missing/wrong keys return 401. Connections have a five-second read timeout.

Re-run real loopback HTTP smoke tests (they use an ephemeral port and stop their servers):

```powershell
python .\tests\test_mock_server.py
```
