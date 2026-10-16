# Staging server on dev.ecowattsense.co.uk

## 1. What runs where

- One Oracle Cloud VM (Ampere, arm64, Ubuntu 24.04).
- Two containers: `server` (FastAPI and SQLite) and `front` (dashboard and proxy for /api).
- `front` is published on 127.0.0.1:8081 only.
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
3. Create the settings: `cp .env.example .env`, then edit it.
4. Start everything once: `scripts/deploy.sh all`
5. Install the timer: `sudo cp scripts/systemd/* /etc/systemd/system/ && sudo systemctl enable --now ecowattsense-deploy.timer`
6. In Cloudflare Zero Trust, add the public hostname `dev.ecowattsense.co.uk` to the existing tunnel. Set the service to `http://host.docker.internal:8081`.

## 4. How a deploy happens

- The timer starts `scripts/deploy.sh auto` every 5 minutes.
- The script fetches `dev` and compares it with the local commit.
- It rebuilds only the service whose folder changed. A change to `docker-compose.yml` or `.env.example` rebuilds both.
- It updates the clone with a fast forward only. It never resets.
- Then it polls `/api/health` for up to 60 seconds. If there is no answer, it exits with an error.
- There are no secrets in GitHub. There is no self-hosted runner, because the repo is public.

## 5. Manual deploy and logs

```bash
scripts/deploy.sh auto --dry-run   # show what would be built
scripts/deploy.sh all              # rebuild both services now
docker compose ps
docker compose logs -f server
journalctl -u ecowattsense-deploy
systemctl list-timers ecowattsense-deploy.timer
```
