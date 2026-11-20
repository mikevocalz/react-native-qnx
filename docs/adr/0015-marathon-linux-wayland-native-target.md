# ADR-0015: Add MarathonOS as a Linux/Wayland target using the existing Qt/Fabric backend

Status: accepted (2026-11-19) for the shared Qt/Fabric reuse, the `marathon` Metro platform and the opt-in Linux/Wayland host. Proposed (not accepted) for signed `.marathon` native-executable delivery, which depends on upstream. Filed as proposed on 2026-11-02.

This record was filed as ADR-0013 (`0013-marathon-linux-wayland-native-target.md`)
on 2026-11-02. ADR-0013 also names the [open-source Qt decision](0013-open-source-qt.md),
which more documents cite, so this record moved to 0015 on 2026-11-11. Number 0014
was held for a Worklets ADR in #113, which closed without merging, so 0014 is
unassigned. The decision text below is unchanged; the sections after Consequences record what was done.

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

## Implementation status (2026-11-19)

Accepted and implemented:

- The `marathon` Metro platform and JS overrides ([PR #34](https://github.com/mikevocalz/react-native-qnx/pull/34), [PR #138](https://github.com/mikevocalz/react-native-qnx/pull/138)).
- The opt-in `rnqnx_marathon_host` executable reusing `rnqnx_react_host`, default OFF ([PR #35](https://github.com/mikevocalz/react-native-qnx/pull/35)).
- Separate QNX and Marathon Nitro v1 adapters over one portable core ([PR #41](https://github.com/mikevocalz/react-native-qnx/pull/41)).
- The development launcher `pnpm qnx marathon` ([PR #93](https://github.com/mikevocalz/react-native-qnx/pull/93) closed as redundant; its commit is in main through PR #96) and the optional Marathon target in `qnx create` ([PR #100](https://github.com/mikevocalz/react-native-qnx/pull/100)).

CI verifies the Linux build, one offscreen frame, both Metro bundles and the Nitro adapter compile. See [NATIVE_HOST](../marathon/NATIVE_HOST.md) for run URLs.

Still gated, and not part of this acceptance:

- A run in a Wayland compositor, on MarathonOS, and on a physical Marathon device.
- A signed `.marathon` package carrying a native executable. Upstream's manifest schema requires a `.qml` entry point (re-checked at `1f845160`), and nothing has been submitted upstream ([UPSTREAM_RFC](../marathon/UPSTREAM_RFC.md)).
- Lifecycle, back, IME, insets and permission mediation.

If upstream rejects a native-executable entry, the fallbacks are a Flatpak route or a development-client-only target. The shared-renderer decision stays valid in both cases.
