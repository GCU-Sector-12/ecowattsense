# Summary data source

The dashboard uses the recorded sample day by default. Set `VITE_SUMMARY_DATA_SOURCE=real` before starting Vite or building the dashboard to call the same-origin `/api/summary` endpoint with the `ews_session` cookie. Other endpoints keep their own mode.

`getSummary(date)` passes a supplied `YYYY-MM-DD` date to the server. With no date, real mode lets the server choose today in UK local time. Sample mode keeps its recorded day. Energy remains in microjoules and UTC bucket timestamps are unchanged; the chart converts each half-hour bucket to average kW. Partial and daylight-saving days are not padded to 48 points.

HTTP and network errors reject the provider promise; they never fall back to sample data. A real admin cookie is needed; the sample login does not create one. Live staging validation depends on EWS_API-18.
