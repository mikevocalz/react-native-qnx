# ADR-0015: Add MarathonOS as a Linux/Wayland target using the existing Qt/Fabric backend

Status: proposed (2026-11-02)

This record was filed as ADR-0013 (`0013-marathon-linux-wayland-native-target.md`)
on 2026-11-02. ADR-0013 also names the [open-source Qt decision](0013-open-source-qt.md),
which more documents cite, so this record moved to 0015 on 2026-11-11. Number 0014
was held for a Worklets ADR in #113, which closed without merging, so 0014 is
unassigned. The decision text below is unchanged.

## Context

react-native-qnx has an established Qt Quick component layer, Fabric mutation adapter, Hermes/JSI host and QNX-specific Screen/QPA work. MarathonOS uses a Linux Qt Wayland compositor with a separate app runner and security model. Reusing the native Qt/React code is appropriate; transplanting QNX binary or copying shell's QML UI is not.

## Decision

Add an **opt-in Linux/Wayland platform adapter** and Metro platform-specific identity, reusing `rnqnx_views_qt`, `rnqnx_fabric_adapter`, `rnqnx_react_host` as shared source. Keep QNX on its existing source and build flags. Split OS-specific service bindings, bundler overrides and packaging from the shared Qt renderer. Prove development-host Wayland before bundling into signed Marathon app delivery. Work with upstream on signed `.marathon` native executable contracts.

## Rejected alternatives

- New Fabric renderer or QML rewrite: duplicates an already implemented backend and breaks JS parity.
- Run QNX binary on Linux: wrong OS and ABI.
- Android via Deckard: works as an Android route but does not meet native RN/Qt runtime goal.
- WebView shell: not native Fabric/Qt and requires new UI implementation.
- Unconfined distro executable labeled "native Marathon app": skips lifecycle/permissions/security and does not meet out-of-box requirement.

## Consequences / rollback

Architecture continues to depend on Qt Quick and RN 0.88-compatible Fabric/Hermes. Linux and QNX native modules must be compiled separately and may have different capability support. New feature switches default OFF. Remove Marathon target flags and packages to roll back without modifying QNX. Binary app packaging depends on Marathon maintainer approval.
