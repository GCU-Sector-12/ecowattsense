# Settings data source

The Settings page uses sample settings by default. Set `VITE_SETTINGS_DATA_SOURCE=real` before starting Vite or building the dashboard to call the same-origin `GET /api/admin/settings` and `PUT /api/admin/settings` endpoints with the `ews_session` cookie (contract section 5.12). Other endpoints keep their own mode.

The Settings form groups Reporting, Thresholds and Tariff. Report interval is 60 to 3600 seconds. Ping interval and missed-report cycles are editable positive whole numbers, following the admin API limits. Idle after and Unused after are minutes, stored as seconds; unused must be greater than idle. Electricity price (GBP per kWh) and CO2 (kg per kWh) are 0 or more. Errors show under the field and Save stays disabled while a value is invalid, while saving, or when nothing changed.

The same settings feed the captions on the Dashboard page ("No activity for ..."), because `getSettings()` uses this provider.

The save sends all nine fields. `ping_interval_seconds` and `no_data_after_cycles` come from the form. `settings_version` comes unchanged from the last GET or successful save. If the price or the CO2 factor changed, `tariff_valid_from` is the current time in UTC rounded down to the minute, so a new tariff period starts now and past costs stay as they were. Otherwise it is the value from the last GET.

Answers to a save:

- 200: "Saved" for 3 seconds. The returned settings (with the new `settings_version`) become the form state.
- 422: the server message is shown next to the form. The typed values stay.
- 409: "Settings were changed by someone else. Reload to see them." with a Reload button that reads GET again.
- Network error or another failure: an error with Try again. The typed values stay.

A failed first load, including 401, shows the error card with Retry, as on the Computers page. There is no fallback to sample data.

In real mode the tariff timestamp is read-only: the existing automatic tariff-period rule is preserved. In sample mode a UTC tariff date can be edited; saves stay in memory only and are lost on reload. The header shows the current time in real mode.

Energy rates have a separate sample-only section, with active, idle and unused watts for desktop and laptop. Save energy rates commits valid nonnegative watts to tab memory and shows Energy rates saved for three seconds. It does not send watts in the nine-field admin settings PUT. There is no admin energy-rate endpoint, so real mode shows that limitation without displaying sample watts as live values.

Staging sets `ECOWATTSENSE_SETTINGS_DATA_SOURCE=real` in `deploy/staging.env`. `docker-compose.yml` passes it to the image build as `VITE_SETTINGS_DATA_SOURCE`.
