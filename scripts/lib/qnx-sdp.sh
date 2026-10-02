#!/usr/bin/env bash
# Shared QNX SDP environment activation.
# shellcheck shell=bash

rnqnx_source_sdp_environment() {
  if [[ -n "${QNX_HOST:-}" && -n "${QNX_TARGET:-}" ]]; then
    return
  fi

  local candidates=()
  if [[ -n "${QNX_SDP_ENV:-}" ]]; then
    candidates+=("${QNX_SDP_ENV}")
  fi
  candidates+=(
    "${HOME}/qnx800/qnxsdp-env.sh"
    "${HOME}/qnx80x/qnxsdp-env.sh"
  )

  if [[ -n "${USERNAME:-}" ]]; then
    candidates+=(
      "/c/Users/${USERNAME}/qnx800/qnxsdp-env.sh"
      "/cygdrive/c/Users/${USERNAME}/qnx800/qnxsdp-env.sh"
    )
  fi

  local candidate
  for candidate in "${candidates[@]}"; do
    if [[ -f "${candidate}" ]]; then
      echo "Sourcing QNX SDP environment: ${candidate}"
      # shellcheck disable=SC1090
      source "${candidate}"
      break
    fi
  done

  if [[ -z "${QNX_HOST:-}" || -z "${QNX_TARGET:-}" ]]; then
    echo "ERROR: QNX SDP environment is not active." >&2
    echo "Source qnxsdp-env.sh first, or set QNX_SDP_ENV to its full path." >&2
    return 1
  fi
}
