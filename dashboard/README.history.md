# History data source

The computer history page uses sample history by default. Set `VITE_HISTORY_DATA_SOURCE=real` before starting Vite or building the dashboard to call the same-origin `GET /api/devices/{device_id}/history` endpoint with the `ews_session` cookie (contract section 5.9). Other endpoints keep their own mode.

The date input selects one UTC day. The page asks for `from` = 00:00Z of that day and `to` = 00:00Z of the next day (exclusive). The default is today, and changing the date loads that day. The totals come from the response and are not counted again in the browser.

The energy chart groups the reports into half-hour UTC buckets by timestamp and sums `energy_uj`. Reports on battery have `energy_uj` 0, so those half hours show 0. The timeline draws one block for each report, so a full day of 288 reports (interval 300 s) fits.

Failures never fall back to sample data:

- A day with no reports shows the empty state.
- HTTP 404 shows "Device not found".
- Other errors, including 401, show the error card with Retry, as on the Computers page.

Staging sets `ECOWATTSENSE_HISTORY_DATA_SOURCE=real` in `deploy/staging.env`. `docker-compose.yml` passes it to the image build as `VITE_HISTORY_DATA_SOURCE`.
