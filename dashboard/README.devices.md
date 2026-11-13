# Devices data source

The dashboard uses sample devices by default. Set `VITE_DEVICES_DATA_SOURCE=real` before starting Vite or building the dashboard to call the same-origin `/api/devices` endpoint with the `ews_session` cookie. Other endpoints keep their own mode.

`getDevices()` includes archived records so the existing Computers page can filter them with its Show archived switch. Pass `{ include_archived: false }` to request only current records, and `{ location_id: 1 }` to select a room. API errors reject the provider promise and keep their HTTP status; they never fall back to sample data.

Use the Vite API proxy for local testing. A real admin cookie is needed; the sample login does not create one. Live staging validation depends on EWS_API-18.
