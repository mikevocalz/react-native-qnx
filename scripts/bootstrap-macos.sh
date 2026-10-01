#!/usr/bin/env bash
set -euo pipefail

# scripts/bootstrap-macos.sh
# Prepares a macOS machine for react-native-qnx development.
# Does NOT install or download proprietary QNX components.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
MINIMUM_NODE_VERSION="26.0.0"
REQUIRED_PNPM_VERSION="12.8.1"

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

echo "Bootstrapping react-native-qnx macOS authoring host..."

# Xcode Command Line Tools
if ! command -v clang >/dev/null 2>&1; then
  echo "Installing Xcode Command Line Tools..."
  xcode-select --install || true
  echo "Please re-run this script after Xcode Command Line Tools installation completes."
  exit 1
else
  echo "clang: $(clang --version | head -n1)"
fi

# Homebrew (optional but useful)
if ! command -v brew >/dev/null 2>&1; then
  echo "Homebrew not found. Install it manually if you want to use it for auxiliary tools."
fi

# Node.js
if ! command -v node >/dev/null 2>&1; then
  echo "Node.js not found. Install Node.js ${MINIMUM_NODE_VERSION} or newer."
  exit 1
fi
NODE_VERSION="$(node --version | sed 's/^v//')"
if ! version_at_least "${NODE_VERSION}" "${MINIMUM_NODE_VERSION}"; then
  echo "Node.js ${NODE_VERSION} is too old. Require ${MINIMUM_NODE_VERSION} or newer."
  exit 1
fi
echo "node: $(node --version)"

# pnpm: the exact version is part of the repository's reproducibility contract.
if ! command -v pnpm >/dev/null 2>&1 || [[ "$(pnpm --version)" != "${REQUIRED_PNPM_VERSION}" ]]; then
  echo "Installing pnpm ${REQUIRED_PNPM_VERSION}..."
  if command -v corepack >/dev/null 2>&1; then
    corepack enable
    corepack prepare "pnpm@${REQUIRED_PNPM_VERSION}" --activate
  elif command -v npm >/dev/null 2>&1; then
    npm install --global "pnpm@${REQUIRED_PNPM_VERSION}"
  else
    echo "Neither corepack nor npm is available to install pnpm."
    exit 1
  fi
fi
echo "pnpm: $(pnpm --version)"

# CMake
if ! command -v cmake >/dev/null 2>&1; then
  echo "CMake not found. Install CMake 3.31.9 or newer, e.g. via Homebrew: brew install cmake"
  exit 1
fi
CMAKE_VERSION=$(cmake --version | head -n1 | awk '{print $3}')
if ! version_at_least "${CMAKE_VERSION}" "3.31.9"; then
  echo "CMake ${CMAKE_VERSION} is too old. Require 3.31.9 or newer."
  exit 1
fi
echo "cmake: ${CMAKE_VERSION}"

# Ninja
if ! command -v ninja >/dev/null 2>&1; then
  echo "Ninja not found. Install Ninja, e.g. via Homebrew: brew install ninja"
  exit 1
fi
NINJA_VERSION="$(ninja --version)"
if ! version_at_least "${NINJA_VERSION}" "1.11.0"; then
  echo "Ninja ${NINJA_VERSION} is too old. Require 1.11.0 or newer."
  exit 1
fi
echo "ninja: ${NINJA_VERSION}"

# Python
if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 not found. Install Python 3.11 or newer."
  exit 1
fi
PYTHON_VERSION="$(python3 --version | awk '{print $2}')"
if ! version_at_least "${PYTHON_VERSION}" "3.11.0"; then
  echo "Python ${PYTHON_VERSION} is too old. Require 3.11.0 or newer."
  exit 1
fi
echo "python: ${PYTHON_VERSION}"

# Git
if ! command -v git >/dev/null 2>&1; then
  echo "git not found. Install git."
  exit 1
fi
echo "git: $(git --version)"

cd "${PROJECT_ROOT}"
pnpm install --frozen-lockfile

echo ""
echo "macOS authoring host bootstrap complete."
echo "Next: ./scripts/validate-macos.sh"
