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

### Fixed

- QNX Screen probe initialization cleanup, typed-window setup, render-buffer
  acquisition, ARGB fill attributes, and error reporting.

### Known gaps

- No QNX SDP cross-build or Pi/QSTI runtime evidence exists yet.
- Qt Quick, Hermes, Fabric, Nitro, Expo, Metro, package publishing, and device
  deployment remain behind their documented gates.
