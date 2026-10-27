# Staging server on dev.ecowattsense.co.uk

## 1. What runs where

- One Oracle Cloud VM (Ampere, arm64, Ubuntu 24.04).
- Two containers: `server` (FastAPI and SQLite) and `front` (dashboard and proxy for /api).
- `front` is published on port 8081 on the docker0 address of the VM (172.17.0.1), not on the public address. Only containers on the VM and the VM itself can reach it.
- The Cloudflare Tunnel that already runs on the VM has a public hostname `dev.ecowattsense.co.uk`. It points to `http://host.docker.internal:8081`.
- HTTPS ends at Cloudflare. No port is open to the internet.

## 2. Run it on your own machine

```bash
cp .env.example .env
docker compose up -d --build
```

Open http://127.0.0.1:8081. The API health check is at http://127.0.0.1:8081/api/health.

Stop it with `docker compose down`. The SQLite file stays in `./data`.

## 3. First setup on the VM

1. Clone the repo: `git clone https://github.com/GCU-Sector-12/ecowattsense.git ~/ecowattsense`
2. Go in and switch branch: `cd ~/ecowattsense && git switch dev`
3. Create the settings: `cp .env.example .env`, then edit it. Set `ECOWATTSENSE_UID` and `ECOWATTSENSE_GID` to the output of `id -u` and `id -g`, so the server container can write the SQLite file in `./data`. Set `ECOWATTSENSE_FRONT_BIND` to the docker0 address (`ip -4 addr show docker0`, usually 172.17.0.1), so the tunnel container can reach the front.
4. Start everything once: `scripts/deploy.sh all`
5. Install the timer: `sudo cp scripts/systemd/* /etc/systemd/system/ && sudo systemctl enable --now ecowattsense-deploy.timer`
6. In Cloudflare Zero Trust, add the public hostname `dev.ecowattsense.co.uk` to the existing tunnel. Set the service to `http://host.docker.internal:8081`.
7. Make a deploy key pair: `ssh-keygen -t ed25519 -f deploy_key -C github-actions-deploy`. Keep the private half off the VM.
8. On the VM, add the public key to `~/.ssh/authorized_keys` with a forced command, on one line:
   `command="/home/ubuntu/ecowattsense/scripts/deploy.sh auto",no-pty,no-port-forwarding,no-agent-forwarding,no-X11-forwarding ssh-ed25519 AAAA... github-actions deploy`
9. In GitHub, open Settings, Secrets and variables, Actions. Add three repository secrets: `STAGING_SSH_KEY` (the private key), `STAGING_HOST` and `STAGING_USER`. Optional: `DISCORD_WEBHOOK_URL`, a Discord webhook of the #github channel, for the result message.

## 4. How a deploy happens

- A merge to `dev` starts the workflow Deploy staging within seconds. It only runs when `server/`, `dashboard/`, `docker-compose.yml`, `.env.example` or `scripts/deploy.sh` changed.
- The runner opens one SSH connection and runs `scripts/deploy.sh` on the VM. The forced command ignores what the runner sends.
- The build happens on the VM, not on the runner.
- The script fetches `dev` and rebuilds only the service whose folder changed. A change to `docker-compose.yml` or `.env.example` rebuilds both.
- It updates the clone with a fast forward only. It never resets.
- Then it polls `/api/health` for up to 60 seconds. If there is no answer, it exits with an error and the workflow fails.
- The log is in the Actions tab of the repo. The run also posts one line to the Discord channel #github: result, commit and a link to the run (secret `DISCORD_WEBHOOK_URL`).
- The systemd timer runs the same script every 30 minutes. It is a fallback.
- To start a deploy by hand, use Run workflow on the Deploy staging page (`workflow_dispatch`).

## 5. Manual deploy and logs

```bash
scripts/deploy.sh auto --dry-run   # show what would be built
scripts/deploy.sh all              # rebuild both services now
docker compose ps
docker compose logs -f server
journalctl -u ecowattsense-deploy
systemctl list-timers ecowattsense-deploy.timer
```

## 6. Settings and secrets

Staging gets its server settings from GitHub on every deploy. Nobody edits `~/ecowattsense/.env` on the VM by hand.

- Normal settings live in `deploy/staging.env` in the repo. The deploy applies the file on every deploy. This repository is public, so everything in that file is public too.
- Secrets live in GitHub Actions secrets: `EWS_CLIENT_KEY`, `EWS_ADMIN_USERNAME` and `EWS_ADMIN_PASSWORD`. The workflow sends them over the same SSH connection, on stdin. The first line is the marker `# ews-env v1`. Stdin without the marker is ignored.
- A secret that is empty or not set is not sent.

### Change a normal setting

1. Edit `deploy/staging.env`.
2. Open a pull request to `dev` and merge it.
3. The workflow starts and the deploy applies the new value.

### Add or rotate a secret

1. Run `gh secret set NAME --repo GCU-Sector-12/ecowattsense`.
2. Start the workflow by hand: Actions, Deploy staging, Run workflow.

To add a new secret name, see the header of `.github/workflows/deploy-staging.yml`.

### What the script does

- Only keys that start with `EWS_` or `ECOWATTSENSE_` are accepted. Other lines are skipped.
- The script prints key names only, never values. For example `env: updated EWS_CLIENT_KEY` or `env: unchanged`.
- A value from stdin wins over the same key in `deploy/staging.env`.
- Before a change, the old file is copied to `.env.bak` (mode 600). The new `.env` has mode 600.
- The VM owns these keys and the script never touches them: `ECOWATTSENSE_FRONT_BIND`, `ECOWATTSENSE_FRONT_PORT`, `ECOWATTSENSE_UID` and `ECOWATTSENSE_GID`.

### What happens to the running server

When `.env` changes, the `server` container is recreated, even if no code changed. Admin sessions end, because they live in memory. Users log in again.

### Notes

- First run: the first deploy after this change runs the old `deploy.sh` that is already on the VM. It ignores stdin. Run the workflow a second time.
- `/api` is public through Cloudflare (bypass). Use a long random admin password.
- Tests for the script: `scripts/test-deploy-env.sh`.
