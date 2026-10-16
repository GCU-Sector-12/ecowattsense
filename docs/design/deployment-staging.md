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
9. In GitHub, open Settings, Secrets and variables, Actions. Add three repository secrets: `STAGING_SSH_KEY` (the private key), `STAGING_HOST` and `STAGING_USER`.

## 4. How a deploy happens

- A merge to `dev` starts the workflow Deploy staging within seconds. It only runs when `server/`, `dashboard/`, `docker-compose.yml`, `.env.example` or `scripts/deploy.sh` changed.
- The runner opens one SSH connection and runs `scripts/deploy.sh` on the VM. The forced command ignores what the runner sends.
- The build happens on the VM, not on the runner.
- The script fetches `dev` and rebuilds only the service whose folder changed. A change to `docker-compose.yml` or `.env.example` rebuilds both.
- It updates the clone with a fast forward only. It never resets.
- Then it polls `/api/health` for up to 60 seconds. If there is no answer, it exits with an error and the workflow fails.
- The log is in the Actions tab of the repo.
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
