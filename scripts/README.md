# Scripts

Developer tools: run everything locally, seed the database with test data, build the Windows executable. Not part of the product.

## wifi_check.py

Checks if a Wi-Fi network works with our Cloudflare Tunnel, so we know before the demo (D3, M4). Needs Python 3.8 or newer. It installs and changes nothing.

On a team laptop. A laptop only needs HTTPS (port 443) to the tunnel address:

```bash
python3 scripts/wifi_check.py --client --url https://HOST/
```

On the Mac that runs Docker and cloudflared. This also needs outgoing TCP 7844 to Cloudflare. The script starts a real quick tunnel and fetches a page through it. This takes up to 90 seconds. Add `--no-tunnel` to skip it:

```bash
python3 scripts/wifi_check.py
```

Each line is PASS, WARN, FAIL or SKIP. The last lines say what to do. Plan B for a bad network is the phone hotspot.

Tip: at home run `docker pull cloudflare/cloudflared:latest`, so the image is already on the Mac.

## deploy.sh

Deploys the staging server (dev.ecowattsense.co.uk) from branch `dev`. It runs on the staging VM, by hand or from a systemd timer every 5 minutes.

```bash
scripts/deploy.sh [auto|server|front|all] [--dry-run]
```

`auto` is the default. It fetches `origin/dev`, rebuilds only the service whose folder changed, and checks `/api/health`. `--dry-run` shows what would be built and changes nothing. Setup and logs: `docs/design/deployment-staging.md`.

It also reads settings from `deploy/staging.env` (in `origin/dev`) and from stdin. Stdin counts only when its first line is the marker `# ews-env v1`.
It writes the settings to `.env` and prints key names only. A changed `.env` recreates the `server` container.
`scripts/test-deploy-env.sh` runs the tests for this part.

## check_report.py

Checks report files against `shared/report_schema.json`, so a wrong example or a wrong schema is found before the merge. CI runs it on every pull request. It needs Python 3.8 or newer and two packages:

```bash
pip install -r scripts/requirements-check.txt
```

A file holds one report (a JSON object) or a list of reports. Every report gets one line, `OK` or one `FAIL` line for each problem. The last line is a summary, for example `3 reports checked, 0 failed`.

```bash
python scripts/check_report.py                      # the files in shared/examples
python scripts/check_report.py report.json          # one file
python scripts/check_report.py a.json b.json        # several files
```

Exit codes: `0` all reports are valid, `1` at least one report is invalid, `2` a file or the schema cannot be read (the reason is on stderr).
