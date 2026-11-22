# Post-build Qt evidence and Pages preservation

These steps prepare reviewable, provisional artifacts. They do not publish a
runtime, deploy a site, approve the source offer or change a release gate.

## Preserve and qualify the actual completed build

First require the complete final run to exit zero with its clean-tree preflight
and seven successful stages. Set `RECIPE_SHA` to the full committed recipe SHA
that was archived into `/recipe-final`, not a later application/CI commit. If the
build is restarted with different root paths, update the paths below to that
run. Never append a failed/reused build into the final run's evidence.

The restarted run uses recipe commit
`61d0f25786039aef16a31cb7ca94bc6c03c7b6d1`, recipe directory `/recipe-final`,
roots `/build/final/public` and `/build/final/build`, and six build jobs. Its
outer log is `qt/logs/final-jobs6-single-pass.log` on the external volume.
The inspected container is `rnqnx-qualify-20261122`, Docker context `colima`.
It uses `chroot /task/rootfs`; `/build` is its isolated ext4 filesystem and
`/output` maps to the external host directory
`/Volumes/QNX/clones/rnqnx-software-20261122/qt/rootfs/output`. `/recipe-final` is a
limited source archive and has no `.git`, so `make-source-bundle.sh` cannot run
there unchanged. Use a separate real checkout of the exact recipe commit.

On the host, after setting `RECIPE_SHA`:

```sh
set -euo pipefail
export RECIPE_SHA=61d0f25786039aef16a31cb7ca94bc6c03c7b6d1
: "${RECIPE_SHA:?Set the full committed SHA archived into the build recipe}"
case "$RECIPE_SHA" in *[!0-9a-f]*|'') exit 2;; esac
test "${#RECIPE_SHA}" = 40
export QT_EVIDENCE_HOME=/Volumes/QNX/clones/rnqnx-software-20261122/qt
export QT_RECIPE_ID=${RECIPE_SHA:0:12}
export QT_SOURCE_CHECKOUT="$QT_EVIDENCE_HOME/source-checkout-$QT_RECIPE_ID"
git clone -c core.autocrlf=false --no-hardlinks \
  /tmp/rnqnx-release-gates-20261122 "$QT_SOURCE_CHECKOUT"
git -C "$QT_SOURCE_CHECKOUT" checkout --detach "$RECIPE_SHA"
test -z "$(git -C "$QT_SOURCE_CHECKOUT" status --porcelain)"
python3 - <<'PY'
import hashlib, os, pathlib, subprocess
repo = pathlib.Path(os.environ['QT_SOURCE_CHECKOUT'])
out = pathlib.Path(os.environ['QT_EVIDENCE_HOME']) / 'rootfs/output' / ('recipe-' + os.environ['QT_RECIPE_ID'] + '.sha256')
names = subprocess.check_output(['git', 'ls-files', '-z', '--', 'scripts/qt-public', 'docs/distribution/source-offer.md'], cwd=repo).decode().split('\0')
with out.open('x') as stream:
    for name in filter(None, names):
        path = repo / name
        if path.is_symlink() or not path.is_file():
            raise SystemExit('Unexpected recipe input: ' + name)
        with path.open('rb') as source:
            digest = hashlib.file_digest(source, 'sha256').hexdigest()
        stream.write(f'{digest}  {name}\n')
PY
```

Run the actual audits and stage inside the existing container. The new receipt
directory must not exist. On failure retain it for inspection; use a new
explicit receipt name for any reviewed retry. This keeps audit logs separate
from the build-attempt logs.

```sh
docker --context colima exec -i --env RECIPE_SHA="$RECIPE_SHA" \
  rnqnx-qualify-20261122 chroot /task/rootfs /bin/bash -s <<'CHROOT'
set -euo pipefail
export PATH=/tools/cmake-3.31.10-linux-x86_64/bin:$PATH
export QT_PUBLIC_ROOT=/build/final/public QT_PUBLIC_BUILD=/build/final/build QNX_SDP=/work/qnx800
export QNX_SOFTWARE_GRAB=1
receipt=/output/qualification-${RECIPE_SHA:0:12}
test ! -e "$receipt"
test ! -L "$receipt"
mkdir "$receipt"
cd /recipe-final
sha256sum -c "/output/recipe-${RECIPE_SHA:0:12}.sha256" > "$receipt/recipe-verification.txt"
cd scripts/qt-public
./dump-dynsec.sh > "$receipt/build-dynsec.txt"
test ! -e "$QT_PUBLIC_ROOT/runtime"
test ! -L "$QT_PUBLIC_ROOT/runtime"
export STAGE_LOG_DIR="$receipt/audit-logs"
./audit-and-stage.sh
python3 -I normalize-rpath.py "$QT_PUBLIC_ROOT/runtime" --runpath /data/qt612-public/lib --check > "$receipt/normalize-check.txt"
(cd "$QT_PUBLIC_ROOT/runtime" && find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS)
python3 -I verify-closure.py "$QT_PUBLIC_ROOT/runtime" --runpath-prefix /data/qt612-public/ --json "$receipt/closure.json" > "$receipt/closure.txt"
cp -a "$QT_PUBLIC_ROOT/runtime" "$receipt/runtime"
cp -a "$QT_PUBLIC_ROOT/logs" "$receipt/build-logs"
cp "$QT_PUBLIC_ROOT"/audit-{base,imageformats,svg}.json "$receipt/"
cp "$QT_PUBLIC_BUILD/qnx-build/qtbase/plugins/platforms/libqqnx.so" "$receipt/build-libqqnx.so"
mkdir -p "$receipt/source-input/download/cve-patches" "$receipt/source-input/src/qt-everywhere-src-6.12.0/qtbase"
cp "$QT_PUBLIC_ROOT/download/qt-everywhere-src-6.12.0.tar.xz" "$receipt/source-input/download/"
for patch in CVE-2026-116337-qtbase-6.12-0001.diff CVE-2026-116337-qtbase-6.12-0002.diff CVE-2026-105924-qtbase-6.12.diff; do
  cp "$QT_PUBLIC_ROOT/download/cve-patches/$patch" "$receipt/source-input/download/cve-patches/"
done
cp "$QT_PUBLIC_ROOT/src/qt-everywhere-src-6.12.0/qtbase/.qnx-patches-applied" "$receipt/source-input/src/qt-everywhere-src-6.12.0/qtbase/"
CHROOT
```

If dynamic-section or closure checks fail, stop and diagnose the recipe. Do not
rewrite RPATHs or broaden the system-library allowlist to make the check pass.
Review the actual SPDX classifications and notices; a successful script is not
owner licensing approval. No QNX SDK, board system library or identity material
is copied by this sequence.

Then assemble Corresponding Source and a provisional package manifest on the
host using Python 3.11 or later:

```sh
export QT_RECEIPT="$QT_EVIDENCE_HOME/rootfs/output/qualification-$QT_RECIPE_ID"
mkdir "$QT_RECEIPT/corresponding-source"
QT_PUBLIC_ROOT="$QT_RECEIPT/source-input" QNX_SOFTWARE_GRAB=1 WITH_ARCHIVE=1 \
  bash "$QT_SOURCE_CHECKOUT/scripts/qt-public/make-source-bundle.sh" "$QT_RECEIPT/corresponding-source"
python3 -I "$QT_SOURCE_CHECKOUT/scripts/qt-public/generate-package-manifest.py" \
  --runtime "$QT_RECEIPT/runtime" \
  --source-manifest "$QT_RECEIPT/corresponding-source/rnqnx-qt612-source-bundle/SOURCE-MANIFEST.json" \
  --patch-stamp "$QT_RECEIPT/source-input/src/qt-everywhere-src-6.12.0/qtbase/.qnx-patches-applied" \
  --stages "$QT_RECEIPT/build-logs/stages.tsv" \
  --build-plugin "$QT_RECEIPT/build-libqqnx.so" \
  --output "$QT_RECEIPT/package-manifest.json"
tar -czf "$QT_RECEIPT/runtime.tar.gz" -C "$QT_RECEIPT/runtime" .
shasum -a 256 "$QT_RECEIPT/runtime.tar.gz" "$QT_RECEIPT/package-manifest.json" \
  "$QT_RECEIPT/corresponding-source/rnqnx-qt612-source-bundle.tar.gz" > "$QT_RECEIPT/release-units.sha256"
```

This does not link/run the application, prove the plugin on the QNX VM, approve
the owner source offer/contact, publish units, or perform independent
reproduction. Those remain explicit acceptance requirements. The generator
always leaves `releaseEligible: false`.

### Source preparation completed for this run

The actual source bundle was assembled while the native build continued, once
extraction and patching had completed. Its
[verification receipt](../evidence/source-bundle-2026-11-22/result.json) records
the clean, shallow real checkout at `61d0f257`, all 35 recipe files, all three
CVE patches, the included public Qt archive, and the unchanged draft source
offer. Each recorded file hash was re-read and checked. The 443,549,453-byte
bundle has SHA-256
`e906f4156bd9f402d9b031940415d4d632dbeed917bc28a2aa297c4608cf4946`.

It is retained externally under
`qt/corresponding-source-61d0f2578603/rnqnx-qt612-source-bundle.tar.gz`.
The ready `SOURCE-MANIFEST.json` is inside the adjacent
`rnqnx-qt612-source-bundle/` directory. Source inputs and the actual patch stamp
are under `qt/rootfs/output/source-input-61d0f2578603/`. Reuse these verified
inputs for the post-build generator rather than rerunning commands into their
existing output paths. This is source assembly and hash verification, with
`published: false` and `runtimeQualified: false`; native build completion,
runtime audits and eligibility remain separate.

## Preserve Pages while preparing a frozen candidate

Read-only inspection found the live Pages deployment at commit
`18e708d8b4e87f97eebcfd5b68e082b978227054`, successful run `44983128305`.
Its unexpired `github-pages` artifact is ID `14738706986`, 44,140,595 bytes,
with GitHub digest
`sha256:b69dc370e2d53f0ca3dfee5bcaf45ce9413c3bde5e6e7910b4cec1fb5b0ac186`.
These are point-in-time inputs; recheck the active deployment before preparing
a deployment artifact.

The Pages workflow uploads the entire `out/` tree. Uploading a candidate export
alone would replace the live site. `assemble-published.mjs` instead accepts the
downloaded/extracted live export and a frozen candidate with an independently
recorded `snapshot.json` hash, and writes a new combined export:

```sh
: "${DOCS_SNAPSHOT_SHA256:?Set the independently recorded snapshot.json SHA-256}"
node docs-site/scripts/assemble-published.mjs \
  /path/to/verified-live-export /path/to/frozen-candidate \
  "$DOCS_SNAPSHOT_SHA256" /path/to/new-combined-export
```

It preserves every live file's bytes and all existing snapshots, adds the
candidate only under its built `/v/candidate-<commit>/` path, and refuses a
different copy at the same immutable identifier. It rejects input symlinks,
candidate hash/path/identity disagreement and existing outputs. Its separate
`.assembly.json` receipt records both inventories and `deployed: false`.
Review the combined artifact and active deployment identity before any deploy.

The current main Pages workflow does not retain frozen copies between builds.
Before publishing a durable candidate URL, add a reviewed immutable snapshot
registry/retrieval step to every Pages build. Otherwise the next main push
removes that path. The live version selector must advertise only copies that
have been published; update `versions.json` through that reviewed process.
No Pages deployment or v1 tag is performed by assembly.

An initial local assembly test used the published live artifact and the frozen
`fa255964` candidate artifact from successful run `45065237576`. Both downloaded
ZIP digests matched GitHub's recorded digests. The candidate upload omitted
`.nojekyll` because the artifact uploader excludes hidden files by default.
The assembler correctly refused the incomplete inventory. The original upload
was preserved. A separate **test-only** copy restored that file only after its
recorded hash proved it is empty; the repaired test preserved all 1503 live files
and added 1505 candidate files including its receipt. This repaired copy is not
a qualified final publication artifact. The workflow's hidden-file fix must be
verified by an untouched complete upload before final actual assembly.
