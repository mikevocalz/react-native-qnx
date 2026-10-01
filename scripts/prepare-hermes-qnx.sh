#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

EXPECTED_TAG="hermes-v260318099.0.2"
EXPECTED_COMMIT="4188b63f24026ca62b79dc81d95db122a59c6089"
SOURCE_DIR="${RNQNX_HERMES_SOURCE_DIR:-${PROJECT_ROOT}/third-party/hermes-260318099.0.2}"
RN_TAG_FILE="${PROJECT_ROOT}/node_modules/react-native/sdks/.hermesv1version"

fail() {
  echo "ERROR: $*" >&2
  exit 1
}

command -v git >/dev/null 2>&1 || fail "git is required"
[[ -f "${RN_TAG_FILE}" ]] || fail "Run pnpm install first; missing ${RN_TAG_FILE}"

RN_TAG="$(tr -d '\r\n' <"${RN_TAG_FILE}")"
[[ "${RN_TAG}" == "${EXPECTED_TAG}" ]] ||
  fail "React Native selects ${RN_TAG}; this port is audited for ${EXPECTED_TAG}"

if [[ ! -d "${SOURCE_DIR}/.git" ]]; then
  mkdir -p "$(dirname "${SOURCE_DIR}")"
  git clone --filter=blob:none --no-checkout https://github.com/facebook/hermes.git "${SOURCE_DIR}"
fi

git -C "${SOURCE_DIR}" fetch --depth 1 origin "${EXPECTED_COMMIT}"
git -C "${SOURCE_DIR}" checkout --detach "${EXPECTED_COMMIT}"

ACTUAL_COMMIT="$(git -C "${SOURCE_DIR}" rev-parse HEAD)"
[[ "${ACTUAL_COMMIT}" == "${EXPECTED_COMMIT}" ]] ||
  fail "Hermes checkout is ${ACTUAL_COMMIT}, expected ${EXPECTED_COMMIT}"

# A rerun starts from the audited upstream commit, so a previously applied patch
# cannot silently accumulate local changes.
git -C "${SOURCE_DIR}" reset --hard "${EXPECTED_COMMIT}"
git -C "${SOURCE_DIR}" clean -fdx

node "${PROJECT_ROOT}/scripts/patch-hermes-qnx.mjs" "${SOURCE_DIR}"
git -C "${SOURCE_DIR}" diff --check

printf '%s\n' "${EXPECTED_TAG}" >"${SOURCE_DIR}/.rnqnx-source-tag"
printf '%s\n' "${EXPECTED_COMMIT}" >"${SOURCE_DIR}/.rnqnx-source-commit"

echo "Prepared Hermes QNX source:"
echo "  tag:    ${EXPECTED_TAG}"
echo "  commit: ${EXPECTED_COMMIT}"
echo "  path:   ${SOURCE_DIR}"
echo "  patcher: scripts/patch-hermes-qnx.mjs"
