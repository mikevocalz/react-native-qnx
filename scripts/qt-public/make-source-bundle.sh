#!/usr/bin/env bash
# Assemble the Corresponding Source bundle for the public-source Qt runtime.
#
#   make-source-bundle.sh <out-dir>
#
# Writes <out-dir>/rnqnx-qt612-source-bundle/ and
# <out-dir>/rnqnx-qt612-source-bundle.tar.gz with:
#   SOURCE-MANIFEST.json  archive URL and hashes, applied diffs, patches, script hashes
#   SOURCE-OFFER.md       copy of docs/distribution/source-offer.md
#   cve-patches/          the qtbase diffs apply-cve-patches.sh applies
#   qt-public/            scripts/qt-public/ (build scripts, patches, README)
#   qt-everywhere-src-6.12.0.tar.xz   only when WITH_ARCHIVE=1
#
# The archive is not copied by default: it is 423 MB, it is published at the URL
# below, and the manifest pins it by SHA-256. WITH_ARCHIVE=1 includes it for a
# distribution that cannot rely on the mirror (docs/packaging.md, item 3).
#
# Inputs (same layout as layout.sh, but needs no QNX SDP and runs on the host):
#   QT_PUBLIC_ROOT   holds download/qt-everywhere-src-6.12.0.tar.xz and
#                    download/cve-patches/*.diff
# Needs: bash, tar, python3, and sha256sum or shasum.
set -euo pipefail

out=${1:?usage: make-source-bundle.sh <out-dir>}
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "$here/qnx-patch-set.sh"
repo=$(cd "$here/../.." && pwd)
root=${QT_PUBLIC_ROOT:?set QT_PUBLIC_ROOT to the directory that holds download/}
# When built sources are present, refuse an offer for a different plugin variant.
stamp=$root/src/qt-everywhere-src-6.12.0/qtbase/.qnx-patches-applied
if [ -e "$stamp" ] || [ -L "$stamp" ]; then
  if [ ! -f "$stamp" ] || [ -L "$stamp" ] ||
     [ "$(cat "$stamp")" != "$(printf '%s\n' "${patches[@]}")" ]; then
    echo 'QNX_SOFTWARE_GRAB does not match the applied QNX patch stamp' >&2
    exit 1
  fi
fi
archive_name=qt-everywhere-src-6.12.0.tar.xz
archive=$root/download/$archive_name
archive_sha=98ff4f44bac6ec3e1e62ee2a4316ae0e3d15badb015d753cc0268a20db52f165
diffs=(
  CVE-2026-116337-qtbase-6.12-0001.diff
  CVE-2026-116337-qtbase-6.12-0002.diff
  CVE-2026-105924-qtbase-6.12.diff
)

if command -v sha256sum >/dev/null 2>&1; then
  sha() { sha256sum "$1" | cut -d' ' -f1; }
else
  sha() { shasum -a 256 "$1" | cut -d' ' -f1; }
fi

test -f "$archive" || { echo "missing $archive" >&2; exit 1; }
test "$(sha "$archive")" = "$archive_sha" || { echo "archive hash mismatch" >&2; exit 1; }

# Diff hashes are the ones apply-cve-patches.sh enforces; read them from there so
# the two cannot drift.
for d in "${diffs[@]}"; do
  want=$(grep -E "^[0-9a-f]{64}  $d\$" "$here/apply-cve-patches.sh" | cut -d' ' -f1)
  test -n "$want" || { echo "no hash for $d in apply-cve-patches.sh" >&2; exit 1; }
  test "$(sha "$root/download/cve-patches/$d")" = "$want" || { echo "hash mismatch: $d" >&2; exit 1; }
done

name=rnqnx-qt612-source-bundle
dest=$out/$name
test ! -e "$dest" || { echo "$dest exists" >&2; exit 1; }
mkdir -p "$dest/cve-patches" "$dest/qt-public"
for d in "${diffs[@]}"; do cp "$root/download/cve-patches/$d" "$dest/cve-patches/$d"; done
# scripts/qt-public/ as committed, so the bundle holds what the repository holds.
(cd "$repo" && git ls-files -z scripts/qt-public) | while IFS= read -r -d '' f; do
  mkdir -p "$dest/qt-public/$(dirname "${f#scripts/qt-public/}")"
  cp "$repo/$f" "$dest/qt-public/${f#scripts/qt-public/}"
done
cp "$repo/docs/distribution/source-offer.md" "$dest/SOURCE-OFFER.md"
if [ "${WITH_ARCHIVE:-0}" = 1 ]; then cp "$archive" "$dest/$archive_name"; fi

commit=$(cd "$repo" && git rev-parse HEAD)
BUNDLE=$dest ARCHIVE_SHA=$archive_sha COMMIT=$commit WITH_ARCHIVE=${WITH_ARCHIVE:-0} \
  QNX_PATCHES="$(printf '%s\n' "${patches[@]}")" \
  python3 -I - <<'PY'
import hashlib, json, os, pathlib

bundle = pathlib.Path(os.environ['BUNDLE'])


def digest(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def files(sub):
    base = bundle / sub
    return {str(p.relative_to(bundle)): digest(p) for p in sorted(base.rglob('*')) if p.is_file()}


manifest = {
    'schema': 1,
    'qt': {
        'version': '6.12.0',
        'archive': 'qt-everywhere-src-6.12.0.tar.xz',
        'url': 'https://download.qt.io/official_releases/qt/6.12/6.12.0/single/qt-everywhere-src-6.12.0.tar.xz',
        'sha256': os.environ['ARCHIVE_SHA'],
        'md5': 'ac677cc64158740f257fd37a5b4c5ffb',
        'includedInBundle': os.environ['WITH_ARCHIVE'] == '1',
        'modulesBuilt': ['qtbase', 'qtshadertools', 'qtdeclarative', 'qtimageformats', 'qtsvg'],
    },
    'appliedQtbaseDiffs': {
        'urlBase': 'https://download.qt.io/official_releases/qt/6.12/',
        'files': files('cve-patches'),
    },
    'rnQnxPatches': {
        'softwareGrab': os.environ['QNX_SOFTWARE_GRAB'] == '1',
        'appliedByScript': os.environ['QNX_PATCHES'].splitlines(),
        'notAppliedByScript': [] if os.environ['QNX_SOFTWARE_GRAB'] == '1' else ['qtbase-qnx-software-grab.patch'],
    },
    'recipe': {
        'repository': 'https://github.com/mikevocalz/react-native-qnx',
        'commit': os.environ['COMMIT'],
        'files': files('qt-public'),
    },
}
(bundle / 'SOURCE-MANIFEST.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
PY

tar --no-xattrs -czf "$out/$name.tar.gz" -C "$out" "$name" 2>/dev/null ||
  tar -czf "$out/$name.tar.gz" -C "$out" "$name"
echo "$(sha "$out/$name.tar.gz")  $name.tar.gz" | tee "$out/$name.tar.gz.sha256"
