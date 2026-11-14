# PR 149 native ON evidence

This directory records local source `4d13330ccc18438d4e7a3c38128886feaa6525ca`
and the bounded CMake fixes needed to compile the pinned Worklets/Reanimated
packages on the Linux Clang host. See the
[validation receipt](../../validation/pr149-on-native-build-2026-11-14.md).

- `phase-results.json` distinguishes every actual failed compile from the final
  successful build and 60-case host execution.
- `tested-source-and-artifacts.json` identifies the final source tree, 502
  source-file hashes, compiler configuration, packages and all final artifacts.
- `host-test.log` and `host-test-result.json` record the one CTest target and
  every enabled case. The tested executable stayed unchanged after the final
  Reanimated archive dependency refresh.
- The Worklets/Reanimated fix records and source diff preserve the exact
  omissions, unchanged upstream hashes, generated header content and narrow
  compiler options. Native package contents were not edited.
- `enum-header-dependency-audit.json` captures the initially stale dependencies;
  `enum-header-cache-refresh-final.json` identifies the 15 rebuilt objects;
  `enum-header-dependency-audit-final.json` proves zero remaining old-header
  dependencies across all 113 Reanimated objects.
- `environment-and-source.json` is the initial, pre-adapter source snapshot.
  `tested-source-and-artifacts.json` is the final tested identity. Historical
  phase logs and intermediate flag records are retained as such.

The archives were compiled; an integrated Worklets app was not executed.
The host test uses the existing executable's ASan instrumentation, not UBSan or
whole-dependency instrumentation. QNX/Marathon/device and release gates remain
separate. The binary files themselves are not committed in this directory.
