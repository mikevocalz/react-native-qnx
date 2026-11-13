#!/usr/bin/env bash
# Fetches the pinned Dawn commit into <dir> and applies the QNX patches.
#   scripts/webgpu/fetch-dawn.sh <dir>
# <dir> must not exist yet (or must be an earlier checkout of this script).
# Dawn's own third_party dependencies are fetched by its CMake build
# (DAWN_FETCH_DEPENDENCIES=ON) the first time build-dawn.sh configures.
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
# shellcheck source=scripts/webgpu/dawn-pin.env
source "$here/dawn-pin.env"
dir="${1:?usage: fetch-dawn.sh <dir>}"

if [[ ! -d "$dir/.git" ]]; then
  mkdir -p "$dir"
  git -C "$dir" init -q
  git -C "$dir" remote add origin "$DAWN_URL"
fi
git -C "$dir" fetch -q --depth 1 origin "$DAWN_COMMIT"
git -C "$dir" checkout -q --force FETCH_HEAD
head="$(git -C "$dir" rev-parse HEAD)"
[[ "$head" == "$DAWN_COMMIT" ]] || { echo "ERROR: checked out $head, expected $DAWN_COMMIT" >&2; exit 1; }

for patch in "$root"/patches/webgpu/dawn/*.patch; do
  echo "applying $(basename "$patch")"
  git -C "$dir" apply --whitespace=nowarn "$patch"
done
echo "Dawn $DAWN_COMMIT ready in $dir"
