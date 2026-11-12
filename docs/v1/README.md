# React Native QNX v1 documentation

Draft. This is the documentation set for the v1 release of the React Native
0.88 line. v1 is not released: all eleven gates in the
[readiness record](../releases/v1-readiness.md) are open. Until they pass, each
page below describes the current `main` branch, and every device claim names
the board it ran on.

Gates 8 to 11 come from
[PR #115](https://github.com/mikevocalz/react-native-qnx/pull/115); until it
merges, the readiness record on `main` lists seven.

## Get started

Pick the page for the board you have. The custom Pi 400 image and stock Pi 4
QSTI are different targets with different results.

| You have                        | Start here                                                                                                               |
| ------------------------------- | ------------------------------------------------------------------------------------------------------------------------ |
| A macOS or Linux host, no board | [CLI commands and limits](../getting-started/qnx-cli.md)                                                                 |
| Stock Pi 4 or Pi 5 with QSTI    | [Raspberry Pi today](../getting-started/raspberry-pi-today.md)                                                           |
| The custom Pi 400 image         | [Pi 400 setup](../getting-started/raspberry-pi-400.md), then [runtime bring-up](../getting-started/pi400-hello-world.md) |
| An existing Expo app            | [Create an Expo project for QNX](../getting-started/expo-project-create.md)                                              |
| A QNX SDP host and a board      | [Build and deploy](../getting-started/build-and-deploy.md)                                                               |

## API and support limits

- [Component support](../component-support.md): host, Pi 400 and Pi 4 QSTI
  results in separate columns, with partial props and intentional no-ops.
- [Compatibility matrix](../getting-started/compatibility-matrix.md): pinned
  React Native, React, Expo, Metro, Nitro, Hermes and toolchain versions.
- [Nitro v1 API policy](../platform-modernization/NITRO_V1_API_POLICY.md) and
  [API contracts](../nitro/API_CONTRACTS.md): what native modules can use.
- [Reanimated and Gesture Handler gap](../reanimated-gesture-handler-qnx-map.md):
  why these libraries are not supported yet.

## Evidence

[Evidence gallery](evidence-gallery.md): every receipt, sorted by host, Pi 400
and stock Pi 4 QSTI. The stock Pi 4 column is empty today.

## Decisions

[Architecture decision records](../adr/index.md). ADR-0013 is the open-source Qt
decision; the Marathon Linux/Wayland target ADR is 0015, renumbered from a
duplicate 0013.

## Release notes

[v1 release notes, draft](release-notes.md): pinned dependencies, what v1
covers, and what is still pending.

## Design notes

[Template and conformance app handoff](app-design-handoff.md): colours, sizes
and accessibility findings for the app template and the device test apps.
