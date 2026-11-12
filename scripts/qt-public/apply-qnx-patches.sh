#!/usr/bin/env bash
# Apply this repository's qtbase patches for the qnx platform plugin, after
# apply-cve-patches.sh. See README.md, "QNX platform plugin patch".
# Runs inside qnx805-build as qnxbuild. Refuses to run twice.
set -euo pipefail
source "$(dirname "$0")/layout.sh"
qtbase=$QT_PUBLIC_SRC/qtbase
stamp=$qtbase/.qnx-patches-applied
test ! -e "$stamp"
patches=(qtbase-qnx-offscreen-surface.patch)
cd "$qtbase"
# The extracted tree has no .git of its own. When a parent directory is a git
# repository (the container's /work is one), git apply resolves paths against
# that repository, skips every hunk under qtbase and still exits 0. Stop the
# repository search at qtbase's parent so the patch is applied here.
export GIT_CEILING_DIRECTORIES=${qtbase%/*}
for p in "${patches[@]}"; do
  git apply --check -p1 "$QT_PUBLIC_SCRIPTS/patches/$p"
done
for p in "${patches[@]}"; do
  git apply --verbose -p1 "$QT_PUBLIC_SCRIPTS/patches/$p"
  echo "applied $p"
done
printf '%s\n' "${patches[@]}" > "$stamp"
