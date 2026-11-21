# Wait until an app-emitted line appears in a log file. POSIX sh: the same file
# runs on the host and is piped to the QNX target's sh by scripts/lib/qnx-wait.sh.
# shellcheck shell=sh
#
#   rnqnx_wait_log FILE ERE MIN_COUNT TIMEOUT_S WHAT
#
# Returns 0 as soon as FILE holds at least MIN_COUNT lines matching the
# extended regular expression ERE. Returns 124 after TIMEOUT_S seconds with a
# message that names WHAT, the pattern and how many lines matched. Returns 2 for
# bad arguments. The wait is bounded by counting checks (one per
# RNQNX_WAIT_POLL_S seconds, default 1), so it needs no `date +%s`, which the
# target's date may lack. The interval only spaces the checks. It never stands
# in for a condition: a node that does not appear fails the wait.
#
#   rnqnx_wait_node FILE NAME MIN_COUNT TIMEOUT_S
#
# Waits for MIN_COUNT lines `RNQNX_NODE name=NAME ...`, the line an app prints
# when a named node of its tree exists. NAME is matched literally.

rnqnx_wait_log() {
  if [ "$#" -ne 5 ]; then
    echo "ERROR: rnqnx_wait_log FILE ERE MIN_COUNT TIMEOUT_S WHAT" >&2
    return 2
  fi
  _rw_file=$1
  _rw_re=$2
  _rw_min=$3
  _rw_timeout=$4
  _rw_what=$5
  case "$_rw_min" in '' | *[!0-9]*) echo "ERROR: MIN_COUNT must be a whole number, got '$_rw_min'" >&2; return 2 ;; esac
  case "$_rw_timeout" in '' | *[!0-9]*) echo "ERROR: TIMEOUT_S must be a whole number, got '$_rw_timeout'" >&2; return 2 ;; esac
  _rw_poll=${RNQNX_WAIT_POLL_S:-1}
  case "$_rw_poll" in '' | *[!0-9]*) echo "ERROR: RNQNX_WAIT_POLL_S must be a whole number" >&2; return 2 ;; esac
  if [ "$_rw_min" -lt 1 ] || [ "$_rw_timeout" -lt 1 ] || [ "$_rw_poll" -lt 1 ]; then
    echo "ERROR: MIN_COUNT, TIMEOUT_S and RNQNX_WAIT_POLL_S must be at least 1" >&2
    return 2
  fi
  _rw_checks=0
  _rw_limit=$((_rw_timeout / _rw_poll))
  [ "$_rw_limit" -ge 1 ] || _rw_limit=1
  while :; do
    _rw_seen=$(grep -a -c -E -e "$_rw_re" "$_rw_file" 2>/dev/null)
    case "$_rw_seen" in '' | *[!0-9]*) _rw_seen=0 ;; esac
    if [ "$_rw_seen" -ge "$_rw_min" ]; then
      return 0
    fi
    if [ "$_rw_checks" -ge "$_rw_limit" ]; then
      echo "ERROR: timed out after ${_rw_timeout}s waiting for ${_rw_what}: ${_rw_seen} of ${_rw_min} lines matching '${_rw_re}' in ${_rw_file}" >&2
      return 124
    fi
    _rw_checks=$((_rw_checks + 1))
    sleep "$_rw_poll"
  done
}

rnqnx_wait_node() {
  if [ "$#" -ne 4 ]; then
    echo "ERROR: rnqnx_wait_node FILE NAME MIN_COUNT TIMEOUT_S" >&2
    return 2
  fi
  case "$2" in
    '' | *[!A-Za-z0-9_-]*) echo "ERROR: node name must be letters, digits, _ and -, got '$2'" >&2; return 2 ;;
  esac
  rnqnx_wait_log "$1" "^RNQNX_NODE name=$2( |\$)" "$3" "$4" "node '$2'"
}
