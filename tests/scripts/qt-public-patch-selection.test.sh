#!/usr/bin/env bash
# Host regression: real git apply against synthetic old-side Qt patch hunks.
# This checks patch selection/application, not compilation or target behavior.
set -euo pipefail
scripts=$(cd "$(dirname "$0")/../../scripts/qt-public" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
export QT_PUBLIC_ROOT="$tmp/public" QT_PUBLIC_BUILD="$tmp/build"
qtbase="$QT_PUBLIC_ROOT/src/qt-everywhere-src-6.12.0/qtbase"
stamp="$qtbase/.qnx-patches-applied"
patches=(qtbase-qnx-offscreen-surface.patch qtbase-qnx-resize-swap.patch qtbase-qnx-software-grab.patch)

fail() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }

fixture() {
  rm -rf "$QT_PUBLIC_ROOT"
  python3 - "$scripts/patches" "$qtbase" <<'PY'
import pathlib
import re
import sys

source, target = map(pathlib.Path, sys.argv[1:])
files = {}
for name in ("offscreen-surface", "resize-swap", "software-grab"):
    path = None
    position = None
    for line in (source / f"qtbase-qnx-{name}.patch").read_text().splitlines():
        if line.startswith("--- a/"):
            path, position = line[6:], None
        elif line.startswith("@@ "):
            position = int(re.match(r"@@ -(\d+)", line)[1]) - 1
        elif position is not None and line[:1] in (" ", "-"):
            original = files.setdefault(path, {})
            if position in original and original[position] != line[1:]:
                raise AssertionError(f"inconsistent old hunk: {path}:{position + 1}")
            original[position] = line[1:]
            position += 1
for name, original in files.items():
    path = target / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(original.get(i, "// synthetic fixture filler")
                              for i in range(max(original) + 1)) + "\n")
PY
}

snapshot() {
  python3 - "$qtbase" <<'PY'
import hashlib
import pathlib
import sys
root = pathlib.Path(sys.argv[1])
for path in sorted(root.rglob("*")):
    if path.is_file():
        print(path.relative_to(root), hashlib.sha256(path.read_bytes()).hexdigest())
PY
}

run_apply() {
  if [[ $1 == unset ]]; then
    env -u QNX_SOFTWARE_GRAB bash "$scripts/apply-qnx-patches.sh"
  else
    QNX_SOFTWARE_GRAB=$1 bash "$scripts/apply-qnx-patches.sh"
  fi
}

for selection in unset 0 1; do
  fixture
  run_apply "$selection" >"$tmp/output" 2>&1 || { cat "$tmp/output" >&2; fail "$selection rejected"; }
  count=2
  if [[ $selection == 1 ]]; then count=3; fi
  printf '%s\n' "${patches[@]:0:$count}" >"$tmp/expected-stamp"
  cmp "$tmp/expected-stamp" "$stamp" || fail "$selection stamp does not name exactly the applied patches"
  (
    cd "$qtbase"
    export GIT_CEILING_DIRECTORIES="${qtbase%/*}"
    for ((i=0; i<count; i++)); do
      git apply --reverse --check -p1 "$scripts/patches/${patches[$i]}"
    done
    if [[ $count == 2 ]]; then
      git apply --check -p1 "$scripts/patches/${patches[2]}"
      if git apply --reverse --check -p1 "$scripts/patches/${patches[2]}" 2>/dev/null; then
        fail 'default applied the optional software-grab patch'
      fi
    fi
  ) || fail "$selection selected patch content mismatch"
  snapshot >"$tmp/before"
  if run_apply "$selection" >"$tmp/output" 2>&1; then fail "$selection accepted repeat application"; fi
  snapshot >"$tmp/after"
  cmp "$tmp/before" "$tmp/after" || fail 'repeat application changed inputs'
done

for kind in regular symlink dangling; do
  fixture
  case $kind in
    regular) printf '%s\n' "${patches[@]:0:2}" >"$stamp" ;;
    symlink)
      printf '%s\n' "${patches[@]:0:2}" >"$tmp/applied-stamp"
      ln -s "$tmp/applied-stamp" "$stamp"
      ;;
    dangling) ln -s "$tmp/missing-applied-stamp" "$stamp" ;;
  esac
  snapshot >"$tmp/before"
  if run_apply 0 >"$tmp/output" 2>&1; then fail "accepted $kind stamp on pristine source"; fi
  snapshot >"$tmp/after"
  cmp "$tmp/before" "$tmp/after" || fail "$kind stamp refusal changed pristine source"
done

for selection in '' 2 true false yes 01 -1 '1 '; do
  fixture
  snapshot >"$tmp/before"
  if run_apply "$selection" >"$tmp/output" 2>&1; then fail "accepted invalid selection '$selection'"; fi
  snapshot >"$tmp/after"
  cmp "$tmp/before" "$tmp/after" || fail "invalid selection '$selection' changed inputs"
done

# Break the last selected patch. Earlier valid patches must not be applied:
# every selected patch must pass --check before the first mutation.
for selection in 0 1; do
  fixture
  if [[ $selection == 0 ]]; then
    printf '// deliberately incompatible\n' >"$qtbase/src/plugins/platforms/qnx/qqnxeglwindow.cpp"
  else
    printf '// deliberately incompatible\n' >"$qtbase/src/plugins/platforms/qnx/qqnxrasterbackingstore.cpp"
  fi
  snapshot >"$tmp/before"
  if run_apply "$selection" >"$tmp/output" 2>&1; then fail "$selection accepted incompatible last patch"; fi
  snapshot >"$tmp/after"
  cmp "$tmp/before" "$tmp/after" || fail "$selection applied a patch before all checks passed"
  test ! -e "$stamp" || fail 'failed preflight wrote a stamp'
done

# Source-offer preflight must reject the wrong variant before reading archives
# or writing an output. No full source-bundle or manifest generation is claimed.
for selection in 0 1; do
  fixture
  other=$((1-selection))
  run_apply "$other" >"$tmp/output" 2>&1
  snapshot >"$tmp/before"
  if QNX_SOFTWARE_GRAB=$selection bash "$scripts/make-source-bundle.sh" "$tmp/offer" >"$tmp/output" 2>&1; then
    fail "$selection source offer accepted the other variant's stamp"
  fi
  grep -Fq 'does not match the applied QNX patch stamp' "$tmp/output" || fail 'source-offer variant check did not precede archive checks'
  test ! -e "$tmp/offer" || fail 'mismatched source offer wrote output'
  snapshot >"$tmp/after"
  cmp "$tmp/before" "$tmp/after" || fail 'source-offer mismatch changed source inputs'
done
for target in "$tmp/absent-stamp" "$tmp/regular-stamp"; do
  fixture
  printf '%s\n' "${patches[@]:0:2}" >"$tmp/regular-stamp"
  ln -s "$target" "$stamp"
  if QNX_SOFTWARE_GRAB=0 bash "$scripts/make-source-bundle.sh" "$tmp/offer" >"$tmp/output" 2>&1; then
    fail 'source offer accepted a symlink patch stamp'
  fi
  grep -Fq 'does not match the applied QNX patch stamp' "$tmp/output" || fail 'source-offer symlink check did not precede archive checks'
  test ! -e "$tmp/offer" || fail 'source offer with a symlink stamp wrote output'
done
printf '%s\n' 'PASS: real git apply; default/0/1 selection, exact stamps, repeat refusal, 9 invalid values, preflight before mutation; source-offer mismatch/symlink preflight only'
