#!/usr/bin/env bash
# Tests for the settings sync in scripts/deploy.sh. Fake values only.
# Usage: bash scripts/test-deploy-env.sh
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=deploy.sh
source "$HERE/deploy.sh"
set +eu

TEST_ROOT="$(mktemp -d)"
trap 'rm -rf "$TEST_ROOT"' EXIT
PASSED=0
FAILED=0
MARKER="# ews-env v1"
GIT_ID=(-c user.name=test -c user.email=test@example.com -c commit.gpgsign=false)

pass() { PASSED=$((PASSED + 1)); echo "PASS $1"; }

fail() {
  FAILED=$((FAILED + 1))
  echo "FAIL $1"
  sed 's/^/    /' "$TEST_ROOT/last.log"
}

run_case() {
  if ( "$2" ) > "$TEST_ROOT/last.log" 2>&1; then pass "$1"; return 0; fi
  fail "$1"
}

new_workdir() {
  WORK="$(mktemp -d "$TEST_ROOT/case.XXXXXX")"
  cd "$WORK" || exit 1
  ENV_CHANGED=0
}

write_env() { printf '%s\n' "$@" > .env; }
write_input() { printf '%s\n' "$MARKER" "$@" > in.txt; }
run_sync() { sync_env < in.txt > out.txt 2>&1; }
mode_of() { stat -c %a "$1" 2>/dev/null || stat -f %Lp "$1"; }
need() { "$@" || { echo "check failed: $*"; return 1; }; }
same_text() { [ "$(cat "$1")" = "$2" ]; }
has_line() { grep -qxF -- "$2" "$1"; }
has_text() { grep -qF -- "$2" "$1"; }

case_append() {
  new_workdir
  write_env "# top" "OTHER=x"
  write_input "EWS_CLIENT_KEY=fake-key-123"
  run_sync
  need same_text .env $'# top\nOTHER=x\nEWS_CLIENT_KEY=fake-key-123' || return 1
  need has_text out.txt "env: updated EWS_CLIENT_KEY" || return 1
  need [ "$ENV_CHANGED" -eq 1 ]
}

case_replace_in_place() {
  new_workdir
  write_env "# top" "EWS_A=old" "OTHER=x" "EWS_B=2"
  write_input "EWS_A=new"
  run_sync
  need same_text .env $'# top\nEWS_A=new\nOTHER=x\nEWS_B=2'
}

case_collapse_duplicates() {
  new_workdir
  write_env "EWS_A=1" "OTHER=y" "EWS_A=2"
  write_input "EWS_A=3"
  run_sync
  need same_text .env $'EWS_A=3\nOTHER=y'
}

case_unchanged() {
  new_workdir
  write_env "EWS_A=same"
  write_input "EWS_A=same"
  run_sync
  need has_text out.txt "env: unchanged" || return 1
  need [ "$ENV_CHANGED" -eq 0 ] || return 1
  need [ ! -e .env.bak ]
}

case_foreign_key() {
  new_workdir
  write_env "OTHER=x"
  write_input "PATH=fake-path" "OTHER_KEY=1"
  run_sync
  need same_text .env "OTHER=x" || return 1
  need has_text out.txt "env: skipped line 2" || return 1
  need has_text out.txt "env: skipped line 3"
}

case_protected_key() {
  new_workdir
  write_env "ECOWATTSENSE_FRONT_PORT=9999"
  write_input "ECOWATTSENSE_FRONT_PORT=1234"
  run_sync
  need same_text .env "ECOWATTSENSE_FRONT_PORT=9999" || return 1
  need has_text out.txt "env: skipped protected ECOWATTSENSE_FRONT_PORT"
}

case_no_marker() {
  new_workdir
  write_env "EWS_A=old"
  printf '%s\n' "EWS_A=new" > in.txt
  run_sync
  need same_text .env "EWS_A=old" || return 1
  need has_text out.txt "env: stdin ignored (no marker)"
}

case_odd_value() {
  new_workdir
  write_env "EWS_A=old"
  write_input 'EWS_ADMIN_PASSWORD=ab=c d #e"f'
  run_sync
  need has_line .env 'EWS_ADMIN_PASSWORD=ab=c d #e"f'
}

case_empty_value() {
  new_workdir
  write_env "EWS_A=old"
  write_input "EWS_A="
  run_sync
  need same_text .env "EWS_A=old" || return 1
  need has_text out.txt "env: skipped empty EWS_A"
}

case_dry_run() {
  new_workdir
  write_env "EWS_A=old"
  cp .env before.txt
  write_input "EWS_A=new" "EWS_B=fake-b"
  DRY_RUN=1
  run_sync
  need cmp -s .env before.txt || return 1
  need [ ! -e .env.bak ] || return 1
  need [ "$ENV_CHANGED" -eq 0 ] || return 1
  need has_text out.txt "env: would update EWS_A EWS_B"
}

case_modes() {
  new_workdir
  write_env "EWS_A=old"
  chmod 644 .env
  write_input "EWS_A=new"
  run_sync
  need [ "$(mode_of .env)" = "600" ] || return 1
  need [ -f .env.bak ] || return 1
  need [ "$(mode_of .env.bak)" = "600" ] || return 1
  need has_line .env.bak "EWS_A=old"
}

case_no_secret_printed() {
  new_workdir
  write_env "EWS_A=old"
  write_input "EWS_ADMIN_PASSWORD=fake-secret-xyz-789" "PATH=fake-secret-pw-555" \
    "ECOWATTSENSE_UID=fake-secret-uid-1" "garbage fake-secret-bad-9" "EWS_EMPTY="
  run_sync
  need has_text out.txt "env: updated EWS_ADMIN_PASSWORD" || return 1
  for secret in fake-secret-xyz-789 fake-secret-pw-555 fake-secret-uid-1 fake-secret-bad-9; do
    need [ "$(grep -cF -- "$secret" out.txt)" -eq 0 ] || return 1
  done
}

make_origin_repo() {
  git init -q --bare origin.git
  git init -q work
  cd work || exit 1
  mkdir deploy
  printf '%s\n' "# file" "EWS_A=from-file" "EWS_B=file-only" > deploy/staging.env
  git add deploy/staging.env
  git "${GIT_ID[@]}" commit -q -m "add staging env"
  git remote add origin ../origin.git
  git push -q origin HEAD:refs/heads/dev
  git fetch -q origin
}

case_committed_file() {
  new_workdir
  make_origin_repo
  write_input "EWS_A=from-stdin"
  run_sync
  need has_line .env "EWS_A=from-stdin" || return 1
  need has_line .env "EWS_B=file-only" || return 1
  need [ "$(grep -cF from-file .env)" -eq 0 ] || return 1
  need [ ! -e .env.bak ]
}

case_server_added() {
  new_workdir
  git init -q
  git "${GIT_ID[@]}" commit -q --allow-empty -m first
  OLD="$(git rev-parse HEAD)"
  NEW="$OLD"
  MODE=auto
  ENV_CHANGED=1
  has_containers() { return 0; }
  ( choose_services; echo "$SERVICES" ) > out.txt 2>&1
  need same_text out.txt "server" || return 1
  SERVICES="front"; add_server_if_env_changed
  need [ "$SERVICES" = "front server" ] || return 1
  SERVICES="server"; add_server_if_env_changed
  need [ "$SERVICES" = "server" ] || return 1
  SERVICES=""; add_server_if_env_changed
  need [ -z "$SERVICES" ]
}

run_case "1 new key is appended" case_append
run_case "2 changed value replaced in place" case_replace_in_place
run_case "3 duplicate keys collapse" case_collapse_duplicates
run_case "4 same value is unchanged" case_unchanged
run_case "5 foreign key is skipped" case_foreign_key
run_case "6 protected key is skipped" case_protected_key
run_case "7 stdin without marker is ignored" case_no_marker
run_case "8 odd value stored exactly" case_odd_value
run_case "9 empty value is skipped" case_empty_value
run_case "10 dry run writes nothing" case_dry_run
run_case "11 file modes are 600" case_modes
run_case "12 no secret value is printed" case_no_secret_printed
run_case "13 committed file, stdin wins" case_committed_file
run_case "14 env change adds server, no exit" case_server_added

echo "Passed: $PASSED, failed: $FAILED"
[ "$FAILED" -eq 0 ]
