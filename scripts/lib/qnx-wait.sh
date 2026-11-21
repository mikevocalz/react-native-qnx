#!/usr/bin/env bash
# Run the wait-log.sh functions on the QNX target. Source scripts/lib/qnx-target.sh
# and call rnqnx_target_init first.
# shellcheck shell=bash

_RNQNX_WAIT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# POSIX single-quote a value for the remote sh.
rnqnx_sh_quote() {
  local value=$1 quote="'" escaped
  escaped=${value//"${quote}"/"${quote}\\${quote}${quote}"}
  printf "'%s'" "${escaped}"
}

# rnqnx_wait_log_remote FILE ERE MIN_COUNT TIMEOUT_S WHAT
rnqnx_wait_log_remote() {
  {
    cat "${_RNQNX_WAIT_DIR}/wait-log.sh"
    printf 'rnqnx_wait_log'
    local arg
    for arg in "$@"; do printf ' %s' "$(rnqnx_sh_quote "${arg}")"; done
    printf '\n'
  } | rnqnx_ssh "sh -s"
}

# rnqnx_wait_node_remote FILE NAME MIN_COUNT TIMEOUT_S
rnqnx_wait_node_remote() {
  {
    cat "${_RNQNX_WAIT_DIR}/wait-log.sh"
    printf 'rnqnx_wait_node'
    local arg
    for arg in "$@"; do printf ' %s' "$(rnqnx_sh_quote "${arg}")"; done
    printf '\n'
  } | rnqnx_ssh "sh -s"
}
