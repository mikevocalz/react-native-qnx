# Provisional Qt package manifest assembly, 2026-11-22

This host-only tool assembles release evidence without granting release
eligibility. `generate-package-manifest.py` always emits `status: provisional`
and `releaseEligible: false`. It changes no historical manifest or readiness
gate. Python 3.11 or later is required.

Run after the complete build, SPDX staging and `SHA256SUMS` generation, with
the source bundle assembled using the same `QNX_SOFTWARE_GRAB` choice:

```sh
python3 -I scripts/qt-public/generate-package-manifest.py \
  --runtime "$QT_PUBLIC_ROOT/runtime" \
  --source-manifest /path/to/source-bundle/SOURCE-MANIFEST.json \
  --patch-stamp "$QT_PUBLIC_ROOT/src/qt-everywhere-src-6.12.0/qtbase/.qnx-patches-applied" \
  --stages "$QT_PUBLIC_ROOT/logs/stages.tsv" \
  --build-plugin "$QT_PUBLIC_BUILD/qnx-build/qtbase/plugins/platforms/libqqnx.so" \
  --output /path/to/new-package-manifest.json
```

The output must be new and outside both the runtime and source bundle; its
parent must exist. Inputs must use real paths rather than user-created symlink
aliases. Internal runtime library symlinks are permitted when their targets are
regular files inside the runtime. Existing output is preserved on refusal.

## Checks performed

- Source manifest version, pinned Qt archive identity, full source commit,
  recipe/CVE file hashes, selected patch list and exact applied-patch stamp.
- All seven expected stages, once and in order, successful exits, nonnegative
  durations and disk figures, consistent timestamps and no overlapping stages.
- Exact regular-file and symlink inventory against the staged manifest and
  `SHA256SUMS`, including duplicate and unsafe path rejection.
- Staged plugin bytes equal the actual build-plugin bytes, and ELF files carry
  the aarch64 little-endian identity.
- A fresh invocation of the repository's real closure verifier, not a saved
  successful JSON report. The recorded dependency inventory must match the new
  ELF scan.
- Runtime, source and build-input hashes remain unchanged after verification.

The generated receipt hashes every regular runtime file, including staging
metadata and `SHA256SUMS`. Its counts therefore cover those files too; historical
receipts that counted only staged payloads retain their original meaning.

A stage table proves order and completion, not that the starting trees were
empty, that a binary came from a particular source, or that it runs on a named
target. The output explicitly leaves clean-start attestation, target/plugin
qualification, owner source-offer/contact approval, release-unit publication
and independent reproduction open. It does not attest third-party licensing.

## Host regression evidence

`python3 -I tests/python/test_qt_package_manifest.py` passes twelve test groups,
including both plugin variants. The fixtures execute the actual closure
verifier on minimal synthetic aarch64 ELF headers, synthetic source files and
stage tables. They check source/hash/variant disagreement, malformed stage
sequences, changed plugin despite refreshed sums, fresh closure refusal of SDP
payloads, unsafe/symlink paths, duplicate JSON/sums, missing staged inventory,
wrong architecture, non-ELF libraries, dot-segment runtime search paths,
post-verification mutation and preserved existing output.
No Qt, SDP, QNX VM or physical device is used by these fixtures.

Role: documentation/release tooling agent. Applied the repository's
`qnx-platform-validation` skill: host fixtures remain separate from actual
cross-build and runtime qualification. No native API or dependency changes,
performance claims or hardware operations are included. Independent review and
an actual staged-runtime invocation remain required. Rollback: remove the new
generator and fixture; preserve existing evidence.
