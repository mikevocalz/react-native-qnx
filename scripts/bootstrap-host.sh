#!/usr/bin/env bash
set -euo pipefail

# scripts/bootstrap-host.sh
# Prepares the canonical x86-64 Ubuntu 22.04 build workstation with non-QNX dependencies.
# QNX SDP itself must be installed separately via QNX Software Center.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
REQUIRED_PNPM_VERSION="12.8.1"
MINIMUM_NODE_VERSION="24.3.0"
MINIMUM_CMAKE_VERSION="3.31.9"

version_at_least() {
  local current="$1"
  local minimum="$2"
  local current_parts
  local minimum_parts
  local index
  IFS='.' read -r -a current_parts <<< "${current}"
  IFS='.' read -r -a minimum_parts <<< "${minimum}"
  for index in 0 1 2 3; do
    local current_part=$((10#${current_parts[${index}]:-0}))
    local minimum_part=$((10#${minimum_parts[${index}]:-0}))
    if (( current_part > minimum_part )); then
      return 0
    fi
    if (( current_part < minimum_part )); then
      return 1
    fi
  done
  return 0
}

echo "Bootstrapping react-native-qnx build host..."
echo "Note: This script only installs freely redistributable tools."
echo "      Install QNX SDP 8.0.5 for the current Pi QSTI lane and QNX Software Center separately."

if [[ "$(id -u)" -eq 0 ]]; then
  APT="apt-get"
else
  APT="sudo apt-get"
fi

${APT} update
${APT} install -y \
  build-essential \
  ninja-build \
  git \
  curl \
  wget \
  python3 \
  python3-pip \
  python3-venv \
  pkg-config \
  clang \
  lld \
  zip \
  unzip

if ! command -v cmake >/dev/null 2>&1; then
  echo "CMake ${MINIMUM_CMAKE_VERSION} or newer is required."
  echo "Install a current official Kitware release, then rerun this script."
  exit 1
fi

CMAKE_VERSION="$(cmake --version | head -n1 | grep -Eo '[0-9]+(\.[0-9]+)+' | head -n1)"
if [[ -z "${CMAKE_VERSION}" ]] ||
  ! version_at_least "${CMAKE_VERSION}" "${MINIMUM_CMAKE_VERSION}"; then
  echo "CMake ${CMAKE_VERSION:-unknown} is too old; require ${MINIMUM_CMAKE_VERSION} or newer."
  echo "The floor includes the QNX qcc C++ language-flag fix."
  exit 1
fi

# Node.js 24 LTS via NodeSource or another supported installer.
if ! command -v node >/dev/null 2>&1 ||
  ! version_at_least "$(node --version | sed 's/^v//')" "${MINIMUM_NODE_VERSION}"; then
  echo "Installing Node.js 24 LTS..."
  curl -fsSL https://deb.nodesource.com/setup_24.x | sudo -E bash -
  ${APT} install -y nodejs
fi
if ! version_at_least "$(node --version | sed 's/^v//')" "${MINIMUM_NODE_VERSION}"; then
  echo "Node.js $(node --version) is too old; require ${MINIMUM_NODE_VERSION} or newer."
  exit 1
fi

# pnpm: the exact version is part of the repository's reproducibility contract.
if ! command -v pnpm >/dev/null 2>&1 || [[ "$(pnpm --version)" != "${REQUIRED_PNPM_VERSION}" ]]; then
  echo "Installing pnpm ${REQUIRED_PNPM_VERSION}..."
  npm install --global "pnpm@${REQUIRED_PNPM_VERSION}"
fi
if [[ "$(pnpm --version)" != "${REQUIRED_PNPM_VERSION}" ]]; then
  echo "pnpm must be exactly ${REQUIRED_PNPM_VERSION}."
  exit 1
fi

cd "${PROJECT_ROOT}"
pnpm install --frozen-lockfile

echo ""
echo "Host bootstrap complete."
echo "Next steps:"
echo "  1. Install QNX SDP 8.0.5 through QNX Software Center for the current Pi QSTI and record its exact update/build."
echo "  2. Source the SDP environment script."
echo "  3. Run ./scripts/doctor.sh"
