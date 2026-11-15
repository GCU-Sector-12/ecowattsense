# Settings data source

The Settings page uses sample settings by default. Set `VITE_SETTINGS_DATA_SOURCE=real` before starting Vite or building the dashboard to call the same-origin `GET /api/admin/settings` and `PUT /api/admin/settings` endpoints with the `ews_session` cookie (contract section 5.12). Other endpoints keep their own mode.

The "Settings" card has five fields: report interval (60 to 3600 seconds), Idle after and Unused after (minutes, stored as seconds; unused must be greater than idle), electricity price (GBP per kWh) and CO2 (kg per kWh), both 0 or more. Errors show under the field and Save stays disabled while a value is invalid, while saving, or when nothing changed.

The same settings feed the captions on the Dashboard page ("No activity for ..."), because `getSettings()` uses this provider.

The save sends all nine fields. `ping_interval_seconds`, `no_data_after_cycles` and `settings_version` are sent unchanged from the last GET. If the price or the CO2 factor changed, `tariff_valid_from` is the current time in UTC rounded down to the minute, so a new tariff period starts now and past costs stay as they were. Otherwise it is the value from the last GET.

Answers to a save:

- 200: "Saved" for 3 seconds. The returned settings (with the new `settings_version`) become the form state.
- 422: the server message is shown next to the form. The typed values stay.
- 409: "Settings were changed by someone else. Reload to see them." with a Reload button that reads GET again.
- Network error or another failure: an error with Try again. The typed values stay.

A failed first load, including 401, shows the error card with Retry, as on the Computers page. There is no fallback to sample data.

In sample mode a save stays in memory only and is lost on reload. The header shows the current time in real mode.

Staging sets `ECOWATTSENSE_SETTINGS_DATA_SOURCE=real` in `deploy/staging.env`. `docker-compose.yml` passes it to the image build as `VITE_SETTINGS_DATA_SOURCE`.
