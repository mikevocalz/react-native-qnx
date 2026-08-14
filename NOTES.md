# Engineering Notes

## 2026-08-13 — Specification intake

- The platform specification locks Qt Quick as the view backend, superseding the
  earlier Screen-as-component-backend direction.
- The current Screen wrapper remains useful only as a QNX window-system probe
  and possible future bare-Screen path. It is not evidence for a Qt Quick,
  Fabric, Hermes, Nitro, or Expo implementation.
- G0-A cannot pass in the current macOS arm64 workspace: no QNX SDP environment,
  `qcc`, `q++`, sysroot, Pi/QSTI address, or target credentials are configured.
- QNX's current public CMake guide documents `CMAKE_SYSTEM_NAME=QNX`,
  `CMAKE_SYSTEM_PROCESSOR=aarch64`, `q++`, the
  `gcc_ntoaarch64le` compiler target, and `QNX_TARGET` as the sysroot. The exact
  installed compiler/version list still has to be captured with `qcc -V`.
- Current Qt documentation says Qt 6 supports QNX SDP 8.0/AArch64le and should be
  built against the project's own SDP, but its Qt-for-QNX page describes the
  offering as Qt for Device Creation Professional. That conflicts with the
  specification's assumed LGPL-only deployment posture and triggers a licensing decision
  before G0-B.
- The specification pins Nitro/Nitrogen 0.36.3 while the official registry's current
  pair is 0.36.5. The user's newer compatible-current instruction controls, so
  the workspace remains exactly 0.36.5/0.36.5, and ADR-0008 records the
  divergence.
- CODEOWNERS identities, the permanent npm organization, per-package release
  units, and release-please ownership cannot be inferred from local paths or git
  metadata.
