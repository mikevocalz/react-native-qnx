#!/usr/bin/env bash
set -euo pipefail

# scripts/bootstrap-host.sh
# Prepares an x86-64 Ubuntu 24.04 build workstation with the non-QNX dependencies.
# QNX SDP itself must be installed separately via QNX Software Center.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "Bootstrapping react-native-qnx build host..."
echo "Note: This script only installs freely redistributable tools."
echo "      Install QNX SDP 8.0.5 and QNX Software Center separately."

if [[ "$(id -u)" -eq 0 ]]; then
  APT="apt-get"
else
  APT="sudo apt-get"
fi

${APT} update
${APT} install -y \
  build-essential \
  cmake \
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

# Node.js 24 LTS via NodeSource or other supported installer.
if ! command -v node >/dev/null 2>&1 || [[ "$(node --version | cut -d'v' -f2 | cut -d'.' -f1)" != "24" ]]; then
  echo "Installing Node.js 24 LTS..."
  curl -fsSL https://deb.nodesource.com/setup_24.x | sudo -E bash -
  ${APT} install -y nodejs
fi

# pnpm
if ! command -v pnpm >/dev/null 2>&1; then
  echo "Installing pnpm..."
  corepack enable || true
  npm install -g pnpm@9
fi

echo ""
echo "Host bootstrap complete."
echo "Next steps:"
echo "  1. Install QNX SDP 8.0.5 through QNX Software Center."
echo "  2. Source the SDP environment script."
echo "  3. Run ./scripts/doctor.sh"
