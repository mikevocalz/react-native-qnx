# ADR-0010: macOS is a development host for portable native work

## Status

Accepted 2026-08-14.

## Context

Until now every native seam was treated as blocked behind the licensed QNX SDP
and the Raspberry Pi 4 target. That is correct for QNX-specific seams — Screen,
`qcc`, `libscreen`, `slog2`, SDP sysroot behavior, and anything whose evidence is
target execution. It is wrong for the rest of the stack.

The following authorities are installed on the macOS authoring host today, and a
claim cited against them is exactly as strong as the same claim cited on Linux:

- React Native 0.86.2 C++ sources under `node_modules/react-native/ReactCommon`,
  including the Fabric scheduler, mounting, and UIManager binding seams.
- Yoga, as bundled by that React Native release.
- `react-native-nitro-modules` 0.36.5 and Nitrogen.
- `expo-modules-core` 57.0.10.
- Qt 6.11.1 (Homebrew), providing the same public Qt Quick, QML, and scene-graph
  headers that the pinned Qt LTS will provide on QNX.

Waiting for hardware to write code that never touches a QNX symbol turns a
procurement delay into a development delay.

## Decision

macOS is an approved development and verification host for native work that does
not depend on the QNX toolchain or target. Its results are labeled host evidence
and never promote a QNX or device row.

Rules:

1. Portable code — Fabric mounting, the component-view registry, the Qt Quick
   view layer, Nitro HybridObject cores, the JSI runtime host — is developed and
   tested here first.
2. Platform-specific code stays behind the seam that already isolates it. Nothing
   may call a macOS-only API outside `native/platform/macos-host`.
3. Qt on this host is a development dependency only. The QNX target still builds
   the pinned Qt 6 LTS from source per ADR-0001, and every Qt symbol used here
   must exist in that pinned LTS. Host Qt 6.11.1 is newer than the LTS we expect
   to pin, so using a symbol introduced after the LTS is a defect, not a feature.
4. `find_package(Qt6 ...)` is `QUIET` and optional. A host without Qt still
   configures, builds, and passes every non-Qt target.
5. Host benchmark numbers are recorded as host numbers. G0-C's Pi benchmark is
   not satisfied by them.

## Consequences

Easier: the view layer, mounting layer, and module layer can reach a working
state before the Windows/WSL2 build host and the Pi arrive, so hardware time is
spent on toolchain porting rather than feature development.

Harder: two build hosts now exist, and macOS-isms can leak in. Rules 2 and 3 are
the mitigation, and the QNX cross-build remains the merge gate for anything that
compiles for the target.

Risk accepted: host Qt is a non-LTS Homebrew build with a different QPA plugin
(cocoa/offscreen rather than the QNX plugin over Screen). Window-system behavior
proven here says nothing about the target compositor.

## Alternatives considered

- **Wait for the licensed host.** Rejected: it blocks work that has no QNX
  dependency, and it was the status quo that produced zero native progress.
- **Develop in a Linux container on macOS.** Rejected for now: it adds a runtime
  without removing the QNX gap, since the SDP still cannot be licensed or
  installed there in this workspace. Worth revisiting when the SDP host exists.
- **Build Qt from source on macOS to match the pinned LTS exactly.** Rejected as
  premature: the LTS tag is not selected until G0-B, and Homebrew Qt is
  sufficient for public-API development if rule 3 holds.

## Related files

- `CMakeLists.txt`
- `vnext/include/react-native-qnx/views/QtQuickItemFactory.h`
- `vnext/src/views/qt/QtQuickItemFactory.cpp`
- `vnext/probes/qtquick/main.cpp`
- `docs/adr/0001-qt-quick-backend.md`
- `docs/adr/0002-qt-quick-item-instantiation.md`
- `docs/cited-seams.md`

## Notes

When the pinned Qt LTS is selected in G0-B, re-audit every Qt symbol used by
host-developed code against that tag and record the result in the cited-seams
ledger.
