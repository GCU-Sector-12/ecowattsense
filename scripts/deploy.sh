#!/usr/bin/env bash
# Deploy EcoWattSense staging from branch dev.
# Usage: deploy.sh [auto|server|front|all] [--dry-run]
set -euo pipefail

MODE="auto"
DRY_RUN=0
OLD=""
NEW=""
SERVICES=""

# Settings sync. The VM owns the protected keys, the script never touches them.
ENV_CHANGED=0
ENV_FILE=".env"
ENV_MARKER="# ews-env v1"
ENV_KEY_RE='^((EWS|ECOWATTSENSE)_[A-Z0-9_]+)=(.*)$'
ENV_SKIP_RE='^[[:space:]]*(#.*)?$'
CR="$(printf '\r')"
LINE_NO=0
WANT_FILE=""
NEW_ENV_TMP=""
NAMES_TMP=""

set_arg() {
  case "$1" in
    auto|server|front|all) MODE="$1" ;;
    --dry-run) DRY_RUN=1 ;;
    *) echo "Unknown argument: $1" >&2; exit 2 ;;
  esac
}

parse_args() {
  local arg
  for arg in "$@"; do set_arg "$arg"; done
}

go_to_repo_root() {
  cd "$(dirname "${BASH_SOURCE[0]}")/.."
}

front_port() {
  local port=""
  [ -f .env ] && port="$(grep -E '^ECOWATTSENSE_FRONT_PORT=' .env | tail -n 1 | cut -d= -f2- || true)"
  echo "${port:-8081}"
}

front_bind() {
  local bind=""
  [ -f .env ] && bind="$(grep -E '^ECOWATTSENSE_FRONT_BIND=' .env | tail -n 1 | cut -d= -f2- || true)"
  echo "${bind:-127.0.0.1}"
}

fetch_dev() {
  git fetch origin dev
  OLD="$(git rev-parse HEAD)"
  NEW="$(git rev-parse origin/dev)"
}

is_protected_key() {
  case "$1" in
    ECOWATTSENSE_FRONT_BIND|ECOWATTSENSE_FRONT_PORT|ECOWATTSENSE_UID|ECOWATTSENSE_GID) return 0 ;;
  esac
  return 1
}

cleanup_env_files() {
  rm -f "$WANT_FILE" "$NEW_ENV_TMP" "$NAMES_TMP"
}

prepare_env_files() {
  WANT_FILE="$(umask 077; mktemp)"
  trap cleanup_env_files EXIT
}

# Take one KEY=VALUE line into the wanted file. Prints key names or line numbers only.
parse_setting_line() {
  local n="$1" line="${2%"$CR"}" key value
  [[ "$line" =~ $ENV_SKIP_RE ]] && return 0
  [[ "$line" =~ $ENV_KEY_RE ]] || { echo "env: skipped line $n"; return 0; }
  key="${BASH_REMATCH[1]}"
  value="${BASH_REMATCH[3]}"
  is_protected_key "$key" && { echo "env: skipped protected $key"; return 0; }
  [ -n "$value" ] || { echo "env: skipped empty $key"; return 0; }
  printf '%s=%s\n' "$key" "$value" >> "$WANT_FILE"
}

take_line() {
  LINE_NO=$((LINE_NO + 1))
  parse_setting_line "$LINE_NO" "$1"
}

parse_lines() {
  local line
  LINE_NO=0
  while IFS= read -r line || [ -n "$line" ]; do take_line "$line"; done
}

parse_timed_lines() {
  local line=""
  while IFS= read -r -t 3 line || [ -n "$line" ]; do take_line "$line"; done
}

read_committed_settings() {
  { git show "origin/dev:deploy/staging.env" 2>/dev/null || true; } | parse_lines
}

# Stdin counts only when its first line is the marker. A human waits at most 3 seconds.
read_stdin_settings() {
  local first=""
  IFS= read -r -t 3 first || true
  first="${first%"$CR"}"
  [ -n "$first" ] || return 0
  [ "$first" = "$ENV_MARKER" ] || { echo "env: stdin ignored (no marker)"; return 0; }
  LINE_NO=1
  parse_timed_lines
}

make_private_tmp() {
  (umask 077; mktemp "${1%/}/.ews-env.XXXXXX")
}

env_tmp_dir() {
  [ "$DRY_RUN" -eq 1 ] && { echo "${TMPDIR:-/tmp}"; return 0; }
  dirname "$ENV_FILE"
}

# Merge the wanted file into the current env file. The last wanted value of a key wins.
# Writes the new env to NEW_ENV_TMP and the changed key names to NAMES_TMP.
merge_env() {
  awk -v out="$NEW_ENV_TMP" -v names="$NAMES_TMP" '
    function key_of(s,   i) { i = index(s, "="); return i ? substr(s, 1, i - 1) : "" }
    function val_of(s) { return substr(s, index(s, "=") + 1) }
    function keep(line) { print line > out }
    function mark(k) { changed[k] = 1 }
    function note_want(line,   k) {
      k = key_of(line)
      if (!(k in want)) order[++n] = k
      want[k] = val_of(line)
    }
    function handle_env(line,   k) {
      k = key_of(line)
      if (!(k in want)) { keep(line); return }
      if (k in done) { mark(k); return }
      done[k] = 1
      if (line == (k "=" want[k])) { keep(line); return }
      mark(k)
      keep(k "=" want[k])
    }
    function finish_key(k) {
      if (k in done) return
      keep(k "=" want[k])
      mark(k)
    }
    function print_changed(k) { if (k in changed) print k > names }
    FILENAME == ARGV[1] { note_want($0); next }
    { handle_env($0) }
    END {
      for (i = 1; i <= n; i++) finish_key(order[i])
      for (i = 1; i <= n; i++) print_changed(order[i])
    }
  ' "$WANT_FILE" "$1"
}

backup_env() {
  (umask 077; cp "$ENV_FILE" "$ENV_FILE.bak")
  chmod 600 "$ENV_FILE.bak"
}

install_env() {
  [ ! -f "$ENV_FILE" ] || backup_env
  chmod 600 "$NEW_ENV_TMP"
  mv "$NEW_ENV_TMP" "$ENV_FILE"
  ENV_CHANGED=1
}

report_env() {
  local names
  names="$(tr '\n' ' ' < "$NAMES_TMP")"
  names="${names% }"
  [ -n "$names" ] || { echo "env: unchanged"; return 0; }
  [ "$DRY_RUN" -eq 0 ] || { echo "env: would update $names"; return 0; }
  install_env
  echo "env: updated $names"
}

apply_env() {
  local current="$ENV_FILE"
  [ -s "$WANT_FILE" ] || { echo "env: unchanged"; return 0; }
  [ -f "$current" ] || current=/dev/null
  NEW_ENV_TMP="$(make_private_tmp "$(env_tmp_dir)")"
  NAMES_TMP="$(make_private_tmp "${TMPDIR:-/tmp}")"
  merge_env "$current"
  report_env
}

sync_env() {
  prepare_env_files
  read_committed_settings
  read_stdin_settings
  apply_env
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

select_services() {
  case "$MODE" in
    all) SERVICES="" ; return ;;
    server|front) SERVICES="$MODE" ; return ;;
  esac
  has_containers || { SERVICES="" ; return; }
  [ "$OLD" = "$NEW" ] && [ "$ENV_CHANGED" -eq 0 ] && { echo "nothing new"; exit 0; }
  pick_from_diff
}

# A changed .env needs a new server container, even when no code changed.
add_server_if_env_changed() {
  [ "$ENV_CHANGED" -eq 1 ] || return 0
  [ -n "$SERVICES" ] || return 0
  [ "$SERVICES" != "none" ] || { SERVICES="server"; return 0; }
  case " $SERVICES " in
    *" server "*) ;;
    *) SERVICES="$SERVICES server" ;;
  esac
}

choose_services() {
  select_services
  add_server_if_env_changed
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
  local url="http://$(front_bind):$(front_port)/api/health"
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
  sync_env
  choose_services
  dry_run_exit
  skip_if_none
  git merge --ff-only origin/dev
  # shellcheck disable=SC2086
  docker compose up -d --build $SERVICES
  wait_for_health
  echo "$(date '+%F %T') ${OLD:0:7}..${NEW:0:7} built: $(describe_services)"
}

if [ "${BASH_SOURCE[0]}" = "$0" ]; then main "$@"; fi
