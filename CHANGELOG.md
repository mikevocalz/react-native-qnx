# Changelog

All notable changes will be documented here. Package-specific changelogs begin
when package manifests and independent release units are created.

## Unreleased

### Added

- Repository contribution and handoff rules, including the architecture locks
  and gate order.
- A compatible-current React Native/React/Expo/Metro/Nitro workspace lock and
  installed-source contract validation.
- Host-side C++20 lifecycle tests for the concrete QNX Screen probe with strict
  warnings and sanitizer coverage.

### Changed

- prepare-native always generates the app icon set and `--skip-identity-tool`
  is gone. A `native-app.json` prepared earlier with that option
  (`icon.sizes: []`) is now rejected: delete `build/<platform>/native-app.json`
  and run prepare-native again. `rnqnx project run` on a Windows host now fails
  at prepare-native instead of continuing without an icon set.
- prepare-native reruns the incremental `cmake --build` for the
  `rnqnx_identity_tool` it built in the checkout, so a pull that changes
  `native/app-identity` no longer keeps using a stale binary.

### Fixed

- QNX Screen probe initialization cleanup, typed-window setup, render-buffer
  acquisition, ARGB fill attributes, and error reporting.

### Known gaps

- No QNX SDP cross-build or Pi/QSTI runtime evidence exists yet.
- Qt Quick, Hermes, Fabric, Nitro, Expo, Metro, package publishing, and device
  deployment remain behind their documented gates.
