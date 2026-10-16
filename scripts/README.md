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
