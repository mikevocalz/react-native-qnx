#!/usr/bin/env bash
# Check or restart the macOS QNX SDK build host (Colima VM + qnx805-build
# container). See docs/handoff/QNX_SDK_CONTAINER_MACOS.md.
#
# Subcommands:
#   status  Report each check as PASS or FAIL. Changes nothing.
#   start   Start a stopped Colima profile and a stopped container, then run
#           the same checks.
#
# This script never creates, recreates, removes or reconfigures a Colima
# profile or a container, and it prints no addresses or keys.
set -euo pipefail

profile="${COLIMA_PROFILE:-default}"
container="${QNX_SDK_CONTAINER:-qnx805-build}"
sdp_env="${QNX_SDK_ENV_FILE:-/work/qnx800/qnxsdp-env.sh}"
failures=0

usage() {
  printf 'Usage: %s status|start\n' "${0##*/}" >&2
  exit 2
}

pass() { printf 'PASS  %s\n' "$1"; }
fail() {
  printf 'FAIL  %s\n' "$1"
  failures=$((failures + 1))
}

colima_state() {
  # Prints Running, Stopped or missing for the profile.
  local line
  line="$(colima list 2>/dev/null | awk -v p="$profile" '$1 == p { print $2 }')"
  printf '%s\n' "${line:-missing}"
}

use_profile_socket() {
  local socket="${HOME}/.colima/${profile}/docker.sock"
  export DOCKER_HOST="unix://${socket}"
}

container_state() {
  docker inspect --format '{{.State.Status}}' "$container" 2>/dev/null || printf 'missing\n'
}

check_colima() {
  local state
  state="$(colima_state)"
  if [[ "$state" == "Running" ]]; then
    pass "colima profile '${profile}' is running"
  else
    fail "colima profile '${profile}' is ${state}"
  fi
}

check_container() {
  local state
  state="$(container_state)"
  if [[ "$state" == "running" ]]; then
    pass "container '${container}' is running"
  else
    fail "container '${container}' is ${state}"
  fi
}

check_sdp() {
  if docker exec "$container" test -f "$sdp_env" 2>/dev/null; then
    pass "${sdp_env} exists in the container"
  else
    fail "${sdp_env} not found in the container"
  fi
}

run_checks() {
  check_colima
  if [[ "$(colima_state)" == "Running" ]]; then
    use_profile_socket
    check_container
    if [[ "$(container_state)" == "running" ]]; then
      check_sdp
    fi
  fi
}

start_host() {
  local state
  state="$(colima_state)"
  case "$state" in
    Running) ;;
    Stopped)
      printf 'Starting colima profile %s (existing profile, saved settings)\n' "$profile"
      colima start --profile "$profile"
      ;;
    *)
      # Starting a profile that does not exist would create one.
      fail "colima profile '${profile}' does not exist; this script does not create it"
      return
      ;;
  esac
  use_profile_socket
  state="$(container_state)"
  case "$state" in
    running) ;;
    exited | created)
      printf 'Starting container %s\n' "$container"
      docker start "$container" >/dev/null
      ;;
    *)
      fail "container '${container}' is ${state}; this script does not create it"
      ;;
  esac
}

main() {
  [[ $# -eq 1 ]] || usage
  command -v colima >/dev/null 2>&1 || {
    printf 'FAIL  colima is not installed\n'
    exit 1
  }
  command -v docker >/dev/null 2>&1 || {
    printf 'FAIL  docker CLI is not installed\n'
    exit 1
  }
  case "$1" in
    status) ;;
    start) start_host ;;
    *) usage ;;
  esac
  run_checks
  if ((failures > 0)); then
    printf '%d check(s) failed\n' "$failures"
    exit 1
  fi
  printf 'All checks passed\n'
}

main "$@"
