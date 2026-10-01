#!/usr/bin/env bash
# Shared target connection helpers for QNX Raspberry Pi/QSTI workflows.
# shellcheck shell=bash

rnqnx_target_fail() {
  echo "ERROR: $*" >&2
  return 1
}

rnqnx_target_defaults() {
  : "\${QNX_TARGET_IP:=qnxpi.local}"
  : "\${QNX_TARGET_USER:=qnxuser}"
  : "\${QNX_SSH_PORT:=22}"
  : "\${QNX_SSH_MACS:=hmac-sha2-256}"
  : "\${QNX_SSH_CONNECT_TIMEOUT:=10}"
  : "\${QNX_TARGET_DIR:=/data/home/\${QNX_TARGET_USER}/rnqnx}"

  export QNX_TARGET_IP QNX_TARGET_USER QNX_SSH_PORT QNX_SSH_MACS
  export QNX_SSH_CONNECT_TIMEOUT QNX_TARGET_DIR
}

rnqnx_validate_target_env() {
  [[ "\${QNX_TARGET_IP}" =~ ^[A-Za-z0-9.-]+$ ]] ||
    rnqnx_target_fail "QNX_TARGET_IP must be an IPv4 address or DNS/mDNS hostname." || return 1
  [[ "\${QNX_TARGET_USER}" =~ ^[A-Za-z_][A-Za-z0-9._-]*$ ]] ||
    rnqnx_target_fail "QNX_TARGET_USER contains unsupported characters." || return 1
  [[ "\${QNX_SSH_PORT}" =~ ^[0-9]+$ ]] ||
    rnqnx_target_fail "QNX_SSH_PORT must be numeric." || return 1
  ((10#\${QNX_SSH_PORT} >= 1 && 10#\${QNX_SSH_PORT} <= 65535)) ||
    rnqnx_target_fail "QNX_SSH_PORT must be between 1 and 65535." || return 1
  [[ "\${QNX_TARGET_DIR}" =~ ^/[A-Za-z0-9._/-]+$ ]] ||
    rnqnx_target_fail "QNX_TARGET_DIR must be an absolute path containing only letters, numbers, . _ / and -." || return 1

  if [[ -n "\${QNX_SSH_KEY:-}" && ! -f "\${QNX_SSH_KEY}" ]]; then
    rnqnx_target_fail "QNX_SSH_KEY does not name a readable file: \${QNX_SSH_KEY}" || return 1
  fi
}

rnqnx_prepare_ssh_args() {
  RNQNX_SSH_ARGS=(
    -o "ConnectTimeout=\${QNX_SSH_CONNECT_TIMEOUT}"
    -o "ServerAliveInterval=5"
    -o "ServerAliveCountMax=3"
  )
  RNQNX_SCP_ARGS=(
    -o "ConnectTimeout=\${QNX_SSH_CONNECT_TIMEOUT}"
    -o "ServerAliveInterval=5"
    -o "ServerAliveCountMax=3"
  )

  if [[ -n "\${QNX_SSH_MACS}" ]]; then
    RNQNX_SSH_ARGS+=(-o "MACs=\${QNX_SSH_MACS}")
    RNQNX_SCP_ARGS+=(-o "MACs=\${QNX_SSH_MACS}")
  fi

  if [[ -n "\${QNX_SSH_KEY:-}" ]]; then
    RNQNX_SSH_ARGS+=(-i "\${QNX_SSH_KEY}")
    RNQNX_SCP_ARGS+=(-i "\${QNX_SSH_KEY}")
  fi

  if [[ "\${QNX_SSH_PORT}" != "22" ]]; then
    RNQNX_SSH_ARGS+=(-p "\${QNX_SSH_PORT}")
    RNQNX_SCP_ARGS+=(-P "\${QNX_SSH_PORT}")
  fi

  if [[ -n "\${QNX_SSH_BATCH_MODE:-}" ]]; then
    RNQNX_SSH_ARGS+=(-o "BatchMode=\${QNX_SSH_BATCH_MODE}")
    RNQNX_SCP_ARGS+=(-o "BatchMode=\${QNX_SSH_BATCH_MODE}")
  fi

  if [[ -n "\${QNX_SSH_OPTS:-}" ]]; then
    local extra=()
    # QNX_SSH_OPTS is intentionally shell-word based. Prefer ~/.ssh/config for
    # options that require embedded whitespace.
    # shellcheck disable=SC2206
    extra=(\${QNX_SSH_OPTS})
    RNQNX_SSH_ARGS+=("\${extra[@]}")
    RNQNX_SCP_ARGS+=("\${extra[@]}")
  fi
}

rnqnx_target_init() {
  rnqnx_target_defaults
  rnqnx_validate_target_env
  rnqnx_prepare_ssh_args
}

rnqnx_target_address() {
  printf '%s@%s' "\${QNX_TARGET_USER}" "\${QNX_TARGET_IP}"
}

rnqnx_ssh() {
  ssh "\${RNQNX_SSH_ARGS[@]}" "$(rnqnx_target_address)" "$@"
}

rnqnx_scp_to() {
  local local_path="$1"
  local remote_path="$2"
  scp "\${RNQNX_SCP_ARGS[@]}" "\${local_path}" "$(rnqnx_target_address):\${remote_path}"
}

rnqnx_scp_from() {
  local remote_path="$1"
  local local_path="$2"
  scp "\${RNQNX_SCP_ARGS[@]}" "$(rnqnx_target_address):\${remote_path}" "\${local_path}"
}
