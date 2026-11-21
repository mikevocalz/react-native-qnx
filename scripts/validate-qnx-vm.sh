#!/usr/bin/env bash
# scripts/validate-qnx-vm.sh
#
# Non-interactive validation on the QNX VM: attach to (or boot) the VM, deploy
# the proof runners, run the component captures and the ADR-0021 JS-thread
# test on it, and write a machine-readable result with SHA256SUMS.
#
# Every result from this script is labelled "QNX VM (QEMU, TCG, software
# render)". It is not Pi 400 evidence and not release-board evidence. TCG
# timings in the result are not performance evidence.
#
# Exit status: 0 every step passed, 1 a step failed, 2 bad usage,
# 3 the VM could not be reached or booted.
#
# Reuses scripts/vm-proof/capture.sh and scripts/vm-proof/js-thread.sh, which
# run on the guest. Needs bash 3.2 or newer, ssh, tar and shasum or sha256sum
# on the host; the guest needs tar and openssl, which the VM image has.
#
# Usage: scripts/validate-qnx-vm.sh [options]    (see --help)

set -u
set -o pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

LABEL="QNX VM (QEMU, TCG, software render)"
SCHEMA="rnqnx.qnx-vm-validation/1"

# Host side. Override by exporting before the call.
VM_DIR="${RNQNX_VM_DIR:-/Volumes/QNX/qnxvm}"
VM_HOST="${RNQNX_VM_HOST:-127.0.0.1}"
VM_PORT="${RNQNX_VM_SSH_PORT:-2222}"
VM_USER="${RNQNX_VM_USER:-root}"
VM_KEY="${RNQNX_VM_SSH_KEY:-${VM_DIR}/ssh/id_ed25519}"
VM_PIDFILE="${RNQNX_VM_PIDFILE:-/private/tmp/qnxvm.pid}"
VM_MONITOR="${RNQNX_VM_MONITOR:-/private/tmp/qnxvm.mon}"
VM_BOOT_CMD="${RNQNX_VM_BOOT_CMD:-${VM_DIR}/v1/run.sh -daemonize -pidfile ${VM_PIDFILE}}"
BOOT_TIMEOUT="${RNQNX_VM_BOOT_TIMEOUT:-420}"

# Guest side. These paths exist on the 2026-11-21 VM image.
GUEST_ROOT="${RNQNX_VM_GUEST_ROOT:-/data/vm}"
GUEST_APP="${RNQNX_VM_GUEST_APP:-${GUEST_ROOT}/app}"
GUEST_BUNDLES="${RNQNX_VM_GUEST_BUNDLES:-${GUEST_ROOT}/bundles-2}"
GUEST_JS_THREAD="${RNQNX_VM_GUEST_JS_THREAD:-${GUEST_ROOT}/s2}"

CARDS="${RNQNX_VM_CARDS:-virtualized-list virtualized-section-list button safe-area-view image-background}"
JS_THREAD_EXPECTED="${RNQNX_VM_JS_THREAD_EXPECTED:-11}"
MIN_PNG_BYTES="${RNQNX_VM_MIN_PNG_BYTES:-8000}"
EXPECT_WIDTH="${RNQNX_VM_EXPECT_WIDTH:-1280}"
EXPECT_HEIGHT="${RNQNX_VM_EXPECT_HEIGHT:-800}"
CAPTURE_TIMEOUT="${RNQNX_VM_CAPTURE_TIMEOUT:-900}"
JS_THREAD_TIMEOUT="${RNQNX_VM_JS_THREAD_TIMEOUT:-1200}"

BOOT_MODE="auto"
OUT_DIR=""
STAGE_DIR=""
EXPECT_FILE=""
KEEP_RUNNING=0
FORCE_SHUTDOWN=0
SKIP_CAPTURES=0
SKIP_JS_THREAD=0

usage() {
  cat <<EOF
Usage: ${0##*/} [options]

Runs the VM proofs and writes result.json and SHA256SUMS. Label of every
result: ${LABEL}.

Options:
  --out DIR              Output directory (default build/qnx-vm-validation/<run id>).
  --boot auto|never|always
                         auto (default): attach if ssh answers, boot if not.
                         never: fail when the VM is down.
                         always: shut a running VM down first, then boot it.
  --stage DIR            Deploy built artifacts first. DIR may hold app/,
                         js-thread/ and bundles/; each present directory
                         replaces the matching guest directory for this run.
  --expect-sha256 FILE   Lines "<sha256>  <guest path>"; every guest file named
                         must hash to that value.
  --keep-running         Leave the VM running even if this run booted it.
  --shutdown             Stop the VM at the end even if it was already up.
  --skip-captures        Do not run the component captures.
  --skip-js-thread       Do not run the JS-thread test.
  -h, --help             Show this text.

Environment (defaults in the script header): RNQNX_VM_DIR, RNQNX_VM_HOST,
RNQNX_VM_SSH_PORT, RNQNX_VM_USER, RNQNX_VM_SSH_KEY, RNQNX_VM_PIDFILE,
RNQNX_VM_MONITOR, RNQNX_VM_BOOT_CMD, RNQNX_VM_BOOT_TIMEOUT,
RNQNX_VM_GUEST_ROOT, RNQNX_VM_GUEST_APP, RNQNX_VM_GUEST_BUNDLES,
RNQNX_VM_GUEST_JS_THREAD, RNQNX_VM_CARDS, RNQNX_VM_JS_THREAD_EXPECTED,
RNQNX_VM_MIN_PNG_BYTES.
EOF
}

while [ $# -gt 0 ]; do
  case "$1" in
    --out) OUT_DIR="${2:?--out needs a directory}"; shift 2 ;;
    --boot) BOOT_MODE="${2:?--boot needs auto, never or always}"; shift 2 ;;
    --stage) STAGE_DIR="${2:?--stage needs a directory}"; shift 2 ;;
    --expect-sha256) EXPECT_FILE="${2:?--expect-sha256 needs a file}"; shift 2 ;;
    --keep-running) KEEP_RUNNING=1; shift ;;
    --shutdown) FORCE_SHUTDOWN=1; shift ;;
    --skip-captures) SKIP_CAPTURES=1; shift ;;
    --skip-js-thread) SKIP_JS_THREAD=1; shift ;;
    -h | --help) usage; exit 0 ;;
    *) echo "ERROR: unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
done

case "${BOOT_MODE}" in auto | never | always) ;; *) echo "ERROR: --boot must be auto, never or always." >&2; exit 2 ;; esac
[ -z "${STAGE_DIR}" ] || [ -d "${STAGE_DIR}" ] || { echo "ERROR: --stage is not a directory: ${STAGE_DIR}" >&2; exit 2; }
[ -z "${EXPECT_FILE}" ] || [ -f "${EXPECT_FILE}" ] || { echo "ERROR: --expect-sha256 is not a file: ${EXPECT_FILE}" >&2; exit 2; }
case "${VM_PORT}" in '' | *[!0-9]*) echo "ERROR: RNQNX_VM_SSH_PORT must be numeric." >&2; exit 2 ;; esac
for v in "${GUEST_ROOT}" "${GUEST_APP}" "${GUEST_BUNDLES}" "${GUEST_JS_THREAD}"; do
  case "${v}" in /*) ;; *) echo "ERROR: guest paths must be absolute: ${v}" >&2; exit 2 ;; esac
  case "${v}" in *[!A-Za-z0-9._/-]*) echo "ERROR: guest path has unsupported characters: ${v}" >&2; exit 2 ;; esac
done
for c in ${CARDS}; do
  case "${c}" in *[!a-z0-9-]*) echo "ERROR: bad card id: ${c}" >&2; exit 2 ;; esac
done

for tool in ssh tar; do
  command -v "${tool}" >/dev/null 2>&1 || { echo "ERROR: ${tool} is not available on PATH." >&2; exit 2; }
done
if command -v shasum >/dev/null 2>&1; then
  sha256_of() { shasum -a 256 "$1" | awk '{print $1}'; }
elif command -v sha256sum >/dev/null 2>&1; then
  sha256_of() { sha256sum "$1" | awk '{print $1}'; }
else
  echo "ERROR: neither shasum nor sha256sum is available on PATH." >&2
  exit 2
fi
[ -f "${VM_KEY}" ] || { echo "ERROR: ssh key is not a readable file: ${VM_KEY}" >&2; exit 2; }

RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)-$$"
[ -n "${OUT_DIR}" ] || OUT_DIR="${PROJECT_ROOT}/build/qnx-vm-validation/${RUN_ID}"
mkdir -p "${OUT_DIR}" || { echo "ERROR: cannot create ${OUT_DIR}" >&2; exit 2; }
OUT_DIR="$(cd "${OUT_DIR}" && pwd)"
GUEST_RUN="${GUEST_ROOT}/validate/${RUN_ID}"

SSH_OPTS=(-i "${VM_KEY}" -p "${VM_PORT}" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null
  -o BatchMode=yes -o LogLevel=ERROR -o ServerAliveInterval=15 -o ServerAliveCountMax=40)

# shellcheck disable=SC2029  # the remote command expands on the client on purpose
vm_ssh() { ssh "${SSH_OPTS[@]}" "${VM_USER}@${VM_HOST}" "$@"; }
vm_up() { vm_ssh -o ConnectTimeout=5 'uname -a' >/dev/null 2>&1; }

# with_timeout SECONDS command... ; kills the command when the time runs out.
with_timeout() {
  local secs="$1" pid killer rc
  shift
  "$@" &
  pid=$!
  (sleep "${secs}"; kill -TERM "${pid}" 2>/dev/null) &
  killer=$!
  wait "${pid}"
  rc=$?
  kill "${killer}" 2>/dev/null
  wait "${killer}" 2>/dev/null
  return "${rc}"
}

now() { date +%s; }

json_str() {
  # Quote one string for JSON. Inputs are short single-line values.
  { printf '%s' "$1" | tr '\r\n\t' '   '; printf '\n'; } | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/"/' -e 's/$/"/'
}

STEPS_JSON=""
CAPTURES_JSON=""
JS_RESULTS_JSON=""
HASHES_JSON=""
JS_SUMMARY_JSON="null"
OVERALL=pass
GUEST_UNAME=""
VM_BOOTED_BY_US=false
BOOT_SECONDS=0
ATTACHED=false
STAGED=false
STAGE_REMOTE=""

add_json() { # add_json VARNAME item
  local var="$1" item="$2" cur
  cur="${!var}"
  if [ -z "${cur}" ]; then printf -v "${var}" '%s' "${item}"; else printf -v "${var}" '%s,%s' "${cur}" "${item}"; fi
}

record_step() { # record_step name status seconds detail
  add_json STEPS_JSON "{\"name\":$(json_str "$1"),\"status\":\"$2\",\"seconds\":$3,\"detail\":$(json_str "$4")}"
  [ "$2" = pass ] || OVERALL=fail
  printf '%-10s %-5s %5ss  %s\n' "$1" "$2" "$3" "$4"
}

finish() {
  local rc="$1" finished commit dirty
  finished="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  commit="$(git -C "${PROJECT_ROOT}" rev-parse HEAD 2>/dev/null || echo unknown)"
  dirty=false
  [ -z "$(git -C "${PROJECT_ROOT}" status --porcelain 2>/dev/null)" ] || dirty=true
  cat >"${OUT_DIR}/result.json" <<EOF
{
  "schema": "${SCHEMA}",
  "label": "${LABEL}",
  "notEvidenceFor": ["Pi 400", "release board", "performance"],
  "timingNote": "TCG timings are not performance evidence.",
  "status": "$([ "${rc}" -eq 0 ] && echo pass || echo fail)",
  "exitCode": ${rc},
  "runId": "${RUN_ID}",
  "startedUtc": "${STARTED_UTC}",
  "finishedUtc": "${finished}",
  "repoCommit": "${commit}",
  "repoDirty": ${dirty},
  "guest": {"uname": $(json_str "${GUEST_UNAME}"), "attached": ${ATTACHED}, "bootedByThisRun": ${VM_BOOTED_BY_US}, "bootSeconds": ${BOOT_SECONDS}, "staged": ${STAGED}},
  "steps": [${STEPS_JSON}],
  "captures": [${CAPTURES_JSON}],
  "jsThread": ${JS_SUMMARY_JSON},
  "jsThreadResults": [${JS_RESULTS_JSON}],
  "guestSha256": [${HASHES_JSON}]
}
EOF
  (
    cd "${OUT_DIR}" || exit 1
    # shellcheck disable=SC2094  # SHA256SUMS is excluded from the find
    find . -type f ! -name SHA256SUMS | LC_ALL=C sort | while IFS= read -r f; do
      printf '%s  %s\n' "$(sha256_of "${f}")" "${f}"
    done >SHA256SUMS
  )
  echo
  echo "${LABEL}: $([ "${rc}" -eq 0 ] && echo PASS || echo FAIL) (exit ${rc})"
  echo "result: ${OUT_DIR}/result.json"
  exit "${rc}"
}

cleanup_guest() {
  vm_ssh "rm -rf '${GUEST_RUN}'" >/dev/null 2>&1 || true
  if [ -n "${STAGE_REMOTE}" ]; then vm_ssh "rm -rf '${STAGE_REMOTE}'" >/dev/null 2>&1 || true; fi
}

vm_stop() {
  # Ask the guest to stop, then fall back to the QEMU monitor.
  vm_ssh 'sync; shutdown -b' >/dev/null 2>&1 || true
  local i=0
  while [ "${i}" -lt 60 ]; do
    vm_up || break
    sleep 2
    i=$((i + 1))
  done
  if [ -S "${VM_MONITOR}" ] && command -v nc >/dev/null 2>&1; then
    sleep 3
    printf 'quit\n' | nc -U -w 3 "${VM_MONITOR}" >/dev/null 2>&1 || true
  fi
  sleep 2
}

STARTED_UTC="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "${LABEL}"
echo "run ${RUN_ID}, output ${OUT_DIR}"

# ---------------------------------------------------------------- attach / boot
t0="$(now)"
if [ "${BOOT_MODE}" = always ] && vm_up; then
  echo "stopping the running VM before the boot"
  vm_stop
fi
if vm_up; then
  ATTACHED=true
  record_step attach pass "$(($(now) - t0))" "attached to a running VM"
else
  if [ "${BOOT_MODE}" = never ]; then
    record_step attach fail "$(($(now) - t0))" "VM is down and --boot never was given"
    finish 3
  fi
  if ! command -v qemu-system-aarch64 >/dev/null 2>&1; then
    record_step attach fail "$(($(now) - t0))" "VM is down and qemu-system-aarch64 is not on PATH"
    finish 3
  fi
  # shellcheck disable=SC2086  # the boot command is a word list on purpose
  if ! ${VM_BOOT_CMD} >"${OUT_DIR}/boot.txt" 2>&1; then
    record_step attach fail "$(($(now) - t0))" "boot command failed: ${VM_BOOT_CMD}"
    finish 3
  fi
  VM_BOOTED_BY_US=true
  waited=0
  until vm_up; do
    waited=$((waited + 5))
    if [ "${waited}" -ge "${BOOT_TIMEOUT}" ]; then
      record_step attach fail "$(($(now) - t0))" "ssh did not answer within ${BOOT_TIMEOUT}s after boot"
      finish 3
    fi
    sleep 5
  done
  BOOT_SECONDS="$(($(now) - t0))"
  record_step attach pass "${BOOT_SECONDS}" "booted the VM, ssh answered"
fi

GUEST_UNAME="$(vm_ssh 'uname -a' 2>/dev/null | tr -d '\r')"
printf '%s\n' "${GUEST_UNAME}" >"${OUT_DIR}/guest-uname.txt"
case "${GUEST_UNAME}" in
  *QNX*QEMU_virt*) ;;
  *)
    record_step identity fail 0 "guest is not the QEMU VM: ${GUEST_UNAME}"
    finish 3
    ;;
esac
record_step identity pass 0 "${GUEST_UNAME}"

# ----------------------------------------------------------------------- deploy
t0="$(now)"
deploy_ok=true
deploy_detail=""
vm_ssh "mkdir -p '${GUEST_RUN}/shots' '${GUEST_RUN}/js-thread'" || { deploy_ok=false; deploy_detail="cannot create ${GUEST_RUN};"; }
for runner in capture js-thread; do
  if ! vm_ssh "cat > '${GUEST_ROOT}/${runner}.sh' && chmod 755 '${GUEST_ROOT}/${runner}.sh'" <"${SCRIPT_DIR}/vm-proof/${runner}.sh"; then
    deploy_ok=false
    deploy_detail="${deploy_detail} could not copy ${runner}.sh;"
  fi
done

if [ -n "${STAGE_DIR}" ] && [ "${deploy_ok}" = true ]; then
  STAGE_REMOTE="${GUEST_ROOT}/stage-${RUN_ID}"
  vm_ssh "mkdir -p '${STAGE_REMOTE}'" || deploy_ok=false
  for part in app js-thread bundles; do
    [ -d "${STAGE_DIR}/${part}" ] || continue
    # COPYFILE_DISABLE stops macOS tar from adding AppleDouble "._" members.
    if ! COPYFILE_DISABLE=1 tar -cf - -C "${STAGE_DIR}/${part}" . | vm_ssh "mkdir -p '${STAGE_REMOTE}/${part}' && tar -xf - -C '${STAGE_REMOTE}/${part}'"; then
      deploy_ok=false
      deploy_detail="${deploy_detail} tar push of ${part} failed;"
      continue
    fi
    # Compare every regular file: host hash against guest hash.
    host_list="${OUT_DIR}/stage-${part}.host.sha256"
    guest_list="${OUT_DIR}/stage-${part}.guest.sha256"
    (cd "${STAGE_DIR}/${part}" && find . -type f | LC_ALL=C sort | while IFS= read -r f; do
      printf '%s  %s\n' "$(sha256_of "${f}")" "${f}"
    done) >"${host_list}"
    vm_ssh "cd '${STAGE_REMOTE}/${part}' && find . -type f -exec openssl dgst -sha256 -r {} +" 2>/dev/null |
      sed -e 's/ \*/  /' | LC_ALL=C sort -k2 >"${guest_list}"
    if ! diff -q "${host_list}" "${guest_list}" >/dev/null; then
      deploy_ok=false
      deploy_detail="${deploy_detail} ${part} hashes differ after the push;"
    fi
    case "${part}" in
      app) GUEST_APP="${STAGE_REMOTE}/app" ;;
      js-thread) GUEST_JS_THREAD="${STAGE_REMOTE}/js-thread" ;;
      bundles) GUEST_BUNDLES="${STAGE_REMOTE}/bundles" ;;
    esac
    STAGED=true
  done
fi

# Hash what the run uses, on the guest.
guest_hash_paths="${GUEST_APP}/org.rnqnx.showcase ${GUEST_APP}/libhermesvm.so ${GUEST_JS_THREAD}/qt_react_host_js_thread_qnx_test ${GUEST_JS_THREAD}/libhermesvm.so ${GUEST_ROOT}/capture.sh ${GUEST_ROOT}/js-thread.sh"
for c in ${CARDS}; do guest_hash_paths="${guest_hash_paths} ${GUEST_BUNDLES}/proof-${c}.hbc"; done
guest_hashes="$(vm_ssh "for f in ${guest_hash_paths}; do [ -f \$f ] && openssl dgst -sha256 -r \$f; done" 2>/dev/null | sed -e 's/ \*/  /')"
printf '%s\n' "${guest_hashes}" >"${OUT_DIR}/guest-sha256.txt"
while IFS= read -r line; do
  [ -n "${line}" ] || continue
  add_json HASHES_JSON "{\"path\":$(json_str "${line#*  }"),\"sha256\":\"${line%%  *}\"}"
done <<EOF
${guest_hashes}
EOF
for need in "${GUEST_APP}/org.rnqnx.showcase" "${GUEST_JS_THREAD}/qt_react_host_js_thread_qnx_test"; do
  if ! printf '%s\n' "${guest_hashes}" | grep -q "  ${need}\$"; then
    deploy_ok=false
    deploy_detail="${deploy_detail} missing on the guest: ${need};"
  fi
done
if [ -n "${EXPECT_FILE}" ]; then
  while IFS= read -r line; do
    case "${line}" in '' | '#'*) continue ;; esac
    want="${line%%  *}"
    path="${line#*  }"
    got="$(printf '%s\n' "${guest_hashes}" | awk -v p="${path}" '$2 == p {print $1}')"
    if [ -z "${got}" ]; then
      got="$(vm_ssh "openssl dgst -sha256 -r '${path}'" 2>/dev/null | awk '{print $1}')"
    fi
    if [ "${got}" != "${want}" ]; then
      deploy_ok=false
      deploy_detail="${deploy_detail} sha256 mismatch for ${path} (want ${want}, got ${got:-none});"
    fi
  done <"${EXPECT_FILE}"
fi
if [ "${deploy_ok}" = true ]; then
  nhash="$(printf '%s\n' "${guest_hashes}" | grep -c .)"
  record_step deploy pass "$(($(now) - t0))" "runners copied; ${nhash} guest files hashed$([ "${STAGED}" = true ] && echo '; staged artifacts verified')"
else
  record_step deploy fail "$(($(now) - t0))" "${deploy_detail}"
  cleanup_guest
  finish 1
fi

# --------------------------------------------------------------------- captures
png_check() { # png_check file -> prints "WxH", fails when the file is not a PNG
  local f="$1" magic dims w h
  magic="$(od -An -tx1 -N8 "${f}" | tr -d ' \n')"
  [ "${magic}" = 89504e470d0a1a0a ] || return 1
  dims="$(od -An -tu1 -j16 -N8 "${f}" | tr -s ' ')"
  # shellcheck disable=SC2086
  set -- ${dims}
  w=$(($1 * 16777216 + $2 * 65536 + $3 * 256 + $4))
  h=$(($5 * 16777216 + $6 * 65536 + $7 * 256 + $8))
  printf '%sx%s' "${w}" "${h}"
}

if [ "${SKIP_CAPTURES}" -eq 0 ]; then
  t0="$(now)"
  mkdir -p "${OUT_DIR}/captures"
  # shellcheck disable=SC2086
  with_timeout "${CAPTURE_TIMEOUT}" vm_ssh "sh '${GUEST_ROOT}/capture.sh' '${GUEST_APP}' '${GUEST_BUNDLES}' '${GUEST_RUN}/shots' ${CARDS}" \
    >"${OUT_DIR}/captures/capture-run.txt" 2>&1
  cap_rc=$?
  vm_ssh "tar -cf - -C '${GUEST_RUN}/shots' ." 2>/dev/null | tar -xf - -C "${OUT_DIR}/captures" 2>/dev/null
  cap_ok=true
  cap_detail=""
  [ "${cap_rc}" -eq 0 ] || { cap_ok=false; cap_detail="capture.sh exit ${cap_rc};"; }
  seen_hashes=""
  ncards=0
  for c in ${CARDS}; do
    ncards=$((ncards + 1))
    png="${OUT_DIR}/captures/${c}.png"
    # The repo ignores *.log, so the per-card app log is kept as .log.txt.
    [ ! -f "${OUT_DIR}/captures/${c}.log" ] || mv "${OUT_DIR}/captures/${c}.log" "${OUT_DIR}/captures/${c}.log.txt"
    log="${OUT_DIR}/captures/${c}.log.txt"
    exit_line="$(grep -E "^${c} exit=" "${OUT_DIR}/captures/capture-run.txt" | tail -1)"
    exit_code="$(printf '%s\n' "${exit_line}" | sed -n 's/.* exit=\([0-9]*\).*/\1/p')"
    bytes=0
    dims=""
    sum=""
    item_ok=true
    item_note=""
    if [ -f "${png}" ]; then
      bytes="$(wc -c <"${png}" | tr -d ' ')"
      dims="$(png_check "${png}")" || { item_ok=false; item_note="not a PNG;"; dims=""; }
      sum="$(sha256_of "${png}")"
    else
      item_ok=false
      item_note="no PNG;"
    fi
    [ "${exit_code}" = 0 ] || { item_ok=false; item_note="${item_note} app exit ${exit_code:-none};"; }
    [ "${bytes}" -ge "${MIN_PNG_BYTES}" ] || { item_ok=false; item_note="${item_note} ${bytes} bytes is under ${MIN_PNG_BYTES};"; }
    [ -z "${dims}" ] || [ "${dims}" = "${EXPECT_WIDTH}x${EXPECT_HEIGHT}" ] || { item_ok=false; item_note="${item_note} size ${dims};"; }
    if [ -f "${log}" ]; then
      grep -q "^RNQNX_PLAYGROUND_SCREENSHOT .* ${EXPECT_WIDTH}x${EXPECT_HEIGHT}" "${log}" || { item_ok=false; item_note="${item_note} no screenshot marker in the app log;"; }
      if grep -Eq 'is not implemented in this experimental entry|Unhandled JS|JS exception|Segmentation|SIGSEGV' "${log}"; then
        item_ok=false
        item_note="${item_note} app log shows an error;"
      fi
    else
      item_ok=false
      item_note="${item_note} no app log;"
    fi
    if [ -n "${sum}" ]; then
      case " ${seen_hashes} " in *" ${sum} "*) item_ok=false; item_note="${item_note} PNG identical to another card;" ;; esac
      seen_hashes="${seen_hashes} ${sum}"
    fi
    [ "${item_ok}" = true ] || { cap_ok=false; cap_detail="${cap_detail} ${c}: ${item_note}"; }
    add_json CAPTURES_JSON "{\"card\":$(json_str "${c}"),\"status\":\"$([ "${item_ok}" = true ] && echo pass || echo fail)\",\"appExit\":$(json_str "${exit_code}"),\"bytes\":${bytes},\"size\":$(json_str "${dims}"),\"sha256\":$(json_str "${sum}"),\"png\":$(json_str "captures/${c}.png")}"
  done
  if [ "${cap_ok}" = true ]; then
    record_step captures pass "$(($(now) - t0))" "${ncards} cards rendered at ${EXPECT_WIDTH}x${EXPECT_HEIGHT}, distinct PNGs, app exit 0"
  else
    record_step captures fail "$(($(now) - t0))" "${cap_detail}"
  fi
fi

# -------------------------------------------------------------------- js-thread
if [ "${SKIP_JS_THREAD}" -eq 0 ]; then
  t0="$(now)"
  mkdir -p "${OUT_DIR}/js-thread"
  with_timeout "${JS_THREAD_TIMEOUT}" vm_ssh "sh '${GUEST_ROOT}/js-thread.sh' '${GUEST_JS_THREAD}' '${GUEST_RUN}/js-thread'" \
    >"${OUT_DIR}/js-thread/stdout.txt" 2>&1
  js_rc=$?
  vm_ssh "tar -cf - -C '${GUEST_RUN}/js-thread' ." 2>/dev/null | tar -xf - -C "${OUT_DIR}/js-thread" 2>/dev/null
  [ ! -f "${OUT_DIR}/js-thread/run.log" ] || mv "${OUT_DIR}/js-thread/run.log" "${OUT_DIR}/js-thread/run.txt"
  run_txt="${OUT_DIR}/js-thread/run.txt"
  js_ok=true
  js_detail=""
  passed=0
  failed=0
  summary_line=""
  if [ -f "${run_txt}" ]; then
    passed="$(grep -c '^\[PASS\]' "${run_txt}")"
    failed="$(grep -c '^\[FAIL\]' "${run_txt}")"
    summary_line="$(grep -E '^All [0-9]+ JS-thread tests passed\.' "${run_txt}" | tail -1)"
    while IFS= read -r line; do
      [ -n "${line}" ] || continue
      add_json JS_RESULTS_JSON "$(json_str "${line#\[RESULT\] }")"
    done <<EOF
$(grep '^\[RESULT\]' "${run_txt}")
EOF
  else
    js_ok=false
    js_detail="no run.log from the guest;"
  fi
  [ "${js_rc}" -eq 0 ] || { js_ok=false; js_detail="${js_detail} js-thread.sh exit ${js_rc};"; }
  grep -q '^exit=0$' "${OUT_DIR}/js-thread/stdout.txt" || { js_ok=false; js_detail="${js_detail} test binary did not exit 0;"; }
  [ "${failed}" -eq 0 ] || { js_ok=false; js_detail="${js_detail} ${failed} [FAIL] lines;"; }
  [ "${passed}" -ge "${JS_THREAD_EXPECTED}" ] || { js_ok=false; js_detail="${js_detail} ${passed} [PASS] lines, expected at least ${JS_THREAD_EXPECTED};"; }
  [ -n "${summary_line}" ] || { js_ok=false; js_detail="${js_detail} no 'All N JS-thread tests passed.' line;"; }
  JS_SUMMARY_JSON="{\"status\":\"$([ "${js_ok}" = true ] && echo pass || echo fail)\",\"passLines\":${passed},\"failLines\":${failed},\"expectedAtLeast\":${JS_THREAD_EXPECTED},\"summary\":$(json_str "${summary_line}")}"
  if [ "${js_ok}" = true ]; then
    record_step js-thread pass "$(($(now) - t0))" "${summary_line} ${passed} [PASS], 0 [FAIL]"
  else
    record_step js-thread fail "$(($(now) - t0))" "${js_detail}"
  fi
fi

# --------------------------------------------------------------------- teardown
cleanup_guest
if [ "${FORCE_SHUTDOWN}" -eq 1 ] || { [ "${VM_BOOTED_BY_US}" = true ] && [ "${KEEP_RUNNING}" -eq 0 ]; }; then
  t0="$(now)"
  vm_stop
  if vm_up; then
    record_step shutdown fail "$(($(now) - t0))" "VM still answers ssh after the shutdown request"
  else
    record_step shutdown pass "$(($(now) - t0))" "VM stopped"
  fi
fi

if [ "${OVERALL}" = pass ]; then finish 0; else finish 1; fi
