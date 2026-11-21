#!/usr/bin/env bash
# Extract only the selected public-source subtrees (same set as the evaluation build),
# omitting tests/examples. Runs inside qnx805-build as qnxbuild.
set -euo pipefail
source "$(dirname "$0")/layout.sh"
pub=$QT_PUBLIC_ROOT
archive=$pub/download/qt-everywhere-src-6.12.0.tar.xz
echo "98ff4f44bac6ec3e1e62ee2a4316ae0e3d15badb015d753cc0268a20db52f165  $archive" | sha256sum -c -
dest=$pub/src
test ! -e "$dest/qt-everywhere-src-6.12.0"
mkdir -p "$dest"
top=qt-everywhere-src-6.12.0
# GNU tar lets `*` match `/` by default, so "$top/*.*" below also matched every
# dotted file inside every other submodule (qtsvg, qtmultimedia, ...). Those
# partial directories made the top-level configure try to configure qtsvg and
# stop on a missing host tool. --no-wildcards-match-slash keeps the pattern to
# top-level files.
tar -xJf "$archive" -C "$dest" --no-wildcards-match-slash \
  --exclude="$top/qtbase/tests" --exclude="$top/qtbase/examples" \
  --exclude="$top/qtdeclarative/tests" --exclude="$top/qtdeclarative/examples" \
  --exclude="$top/qtshadertools/tests" --exclude="$top/qtimageformats/tests" \
  --wildcards "$top/.*" "$top/*.*" "$top/configure" "$top/LICENSES" "$top/cmake" "$top/coin" \
  "$top/qtbase" "$top/qtdeclarative" "$top/qtshadertools" "$top/qtimageformats"
cat "$dest/$top/.tag" 2>/dev/null; echo
ls "$dest/$top"
