# Optional server communication

Server communication is disabled by default. Local-only configuration and JSON logs
are unchanged. No server is contacted in that mode.

Environment variables (standard .NET configuration mapping):

| Variable | Meaning |
|---|---|
| `Server__Enabled` | Explicit opt-in, `true` or `false`; default false |
| `Server__BaseUrl` | Server origin, including optional port, without `/api`, credentials, query or fragment |
| `Server__ClientKey` | Client secret used for `X-EWS-Key` |
| `Reporting__DeviceType` | Still required; never inferred |

Provision the real key through the process environment or an untracked secret configuration
provider. Never put it in tracked JSON, command-line arguments, documentation or logs.
The client variable is Server__ClientKey; EWS_CLIENT_KEY names the backend's configuration.
HTTPS is required except for HTTP localhost/loopback development. No TLS validation bypass,
automatic redirects, cookies, or default Windows credentials are enabled.

Before monitoring starts, enabled mode performs one GET /api/settings. It applies only
report_interval_seconds, idle_threshold_seconds and unused_threshold_seconds. All must be
positive integers; unused must exceed idle. Unrelated fields are ignored. Network/read
failure, timeout, HTTP 408/429 or 5xx uses defaults 300/600/3600 (not local overrides), with
a diagnostic. Malformed/invalid settings, redirects and other HTTP errors including
401/403 fail startup. No refresh, ping or retry occurs. Ctrl+C cancels the startup request.

Each completed report receives one POST /api/reports with the exact ReportJson output.
Success requires a single results item with index 0, created/duplicate status, and null
error. Rejections and other failures are logged and dropped. Server error text and bodies
are not logged. A timeout/transport/malformed-response failure can leave acceptance unknown.
Shutdown cancels in-flight requests. No retries, queues, buffering, registration or commands
are implemented. A single HttpClient is reused, with a ten-second deadline covering headers
and body and a 64 KiB response limit.

Sending is awaited in the existing worker, so a slow request can delay the next five-second
observation by up to the request deadline. The next CPU sample still covers the full counter
span; samples are not split or measured twice. There is no background send queue.

Run `pwsh -NoProfile -File ./tests/Test-ServerClient.ps1` from the client directory.
Tests use a fake HTTP handler and a dummy key, never a real server.
