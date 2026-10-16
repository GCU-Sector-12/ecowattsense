#!/usr/bin/env bash
# Deploy EcoWattSense staging from branch dev.
# Usage: deploy.sh [auto|server|front|all] [--dry-run]
set -euo pipefail

MODE="auto"
DRY_RUN=0
OLD=""
NEW=""
SERVICES=""

parse_args() {
  local arg
  for arg in "$@"; do
    case "$arg" in
      auto|server|front|all) MODE="$arg" ;;
      --dry-run) DRY_RUN=1 ;;
      *) echo "Unknown argument: $arg" >&2; exit 2 ;;
    esac
  done
}

go_to_repo_root() {
  cd "$(dirname "${BASH_SOURCE[0]}")/.."
}

front_port() {
  local port=""
  [ -f .env ] && port="$(grep -E '^ECOWATTSENSE_FRONT_PORT=' .env | tail -n 1 | cut -d= -f2- || true)"
  echo "${port:-8081}"
}

fetch_dev() {
  git fetch origin dev
  OLD="$(git rev-parse HEAD)"
  NEW="$(git rev-parse origin/dev)"
}

has_containers() {
  [ -n "$(docker compose ps -q)" ]
}

changed_matches() {
  git diff --name-only "$OLD" "$NEW" | grep -qE "$1"
}

# Pick services from the changed files. Sets SERVICES (empty means all).
pick_from_diff() {
  local picked=""
  changed_matches '^server/' && picked="$picked server"
  changed_matches '^dashboard/' && picked="$picked front"
  changed_matches '^(docker-compose\.yml|\.env\.example)$' && picked="all"
  case "$picked" in
    *all*) SERVICES="" ;;
    *) SERVICES="${picked# }" ;;
  esac
  [ -z "$picked" ] && SERVICES="none"
  return 0
}

choose_services() {
  case "$MODE" in
    all) SERVICES="" ; return ;;
    server|front) SERVICES="$MODE" ; return ;;
  esac
  has_containers || { SERVICES="" ; return; }
  [ "$OLD" = "$NEW" ] && { echo "nothing new"; exit 0; }
  pick_from_diff
}

describe_services() {
  [ -z "$SERVICES" ] && { echo "all"; return; }
  echo "$SERVICES"
}

dry_run_exit() {
  [ "$DRY_RUN" -eq 1 ] || return 0
  echo "dry run: would build $(describe_services) (${OLD:0:7}..${NEW:0:7})"
  exit 0
}

skip_if_none() {
  [ "$SERVICES" = "none" ] || return 0
  git merge --ff-only origin/dev
  echo "$(date '+%F %T') ${OLD:0:7}..${NEW:0:7} no service changed"
  exit 0
}

wait_for_health() {
  local url="http://127.0.0.1:$(front_port)/api/health"
  local i
  for i in $(seq 1 30); do
    curl -fsS "$url" >/dev/null 2>&1 && return 0
    sleep 2
  done
  echo "Health check failed: $url" >&2
  exit 1
}

main() {
  parse_args "$@"
  go_to_repo_root
  fetch_dev
  choose_services
  dry_run_exit
  skip_if_none
  git merge --ff-only origin/dev
  # shellcheck disable=SC2086
  docker compose up -d --build $SERVICES
  wait_for_health
  echo "$(date '+%F %T') ${OLD:0:7}..${NEW:0:7} built: $(describe_services)"
}

main "$@"
