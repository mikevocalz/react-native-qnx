# React Native QNX + MarathonOS - Architecture and proof boundaries

Status: experimental. The Marathon JS platform, the Linux Qt host and the Nitro v1 adapter compile are verified in host CI. The host also created an `xdg_toplevel` with the expected app_id against a generic headless Weston in CI ([run 44646133204](https://github.com/mikevocalz/react-native-qnx/actions/runs/44646133204)). No run on a Marathon shell or a Marathon device exists, so there is **no claim of shipped MarathonOS support**. Last reviewed 2026-11-19. The evidence ledger is [marathon-finish-2026-11-19](../validation/marathon-finish-2026-11-19.md).

## Existing repository evidence

- `rnqnx_views_qt`: Qt Quick view/item factory and image provider.
- `rnqnx_fabric_adapter`: Fabric shadow-view mutation adapter, touch, scrolling and controls.
- `rnqnx_react_host`: Hermes JSI + runtime scheduler + React/Fabric host over Qt.
- `apps/rn-playground`: production Metro QNX bundle and host rendering fixture.
- `packages/metro-config-qnx`, `packages/react-native-qnx`: QNX Metro resolver and platform JS overrides.
- `packages/qnx-nitro-runtime`: existing synchronous QNX Nitro integration.
- `packages/expo-modules-core-qnx`, `expo-qnx-autolinking`: QNX Expo modules bootstrap/autolinking.
- `packages/metro-config-marathon`, `packages/react-native-marathon`: the `marathon` Metro platform and its JS overrides (PR #34, #138).
- `cmake/RnqnxMarathonHost.cmake`, `vnext/probes/playground/main.cpp` (`RNQNX_MARATHON_HOST`): the `rnqnx_marathon_host` executable (PR #35).
- `native/platform/linux-host/nitro`: the Marathon Nitro v1 adapter (`MarathonNitroRuntime`, `MarathonPlatformInfo`, and a binding emitted by `scripts/nitro-platform-codegen.mjs`, not by Nitrogen); it compiles in host CI (PR #41).
- `native/platform/linux-host`: holds only the Nitro adapter. Shell services (lifecycle, back, IME, insets, permissions) are not implemented and wait on upstream contracts.
- `pnpm qnx marathon inspect|run`: the bounded development launcher ([guide](../getting-started/marathon-development-launcher.md)), integrated through PR #96 after PR #93 closed as redundant.
- QNX native Qt runtime: the physical Pi 400 (custom QNX 8.0.0 image, cross-built with SDP 8.0.5) has run the host from 2026-11-07 through PR #149 run 1 on 2026-11-19. See [pi400-host-validation](../evidence/pi400-host-validation-2026-11-07.md) and [pr149-pi400-run1](../evidence/pr149-pi400-run1-2026-11-19/README.md). The stock Pi 4 QSTI board and a QNX VM have not run it (see [pi400-public-qt](../evidence/pi400-public-qt-2026-11-12-b51b49c.md)). Host Qt renderer results are not QNX device results, and neither is a Marathon result.

## Marathon upstream evidence (patrickjquinn/Marathon-Shell)

- Marathon is a Linux/postmarketOS mobile OS with Qt 6 Wayland compositor and app shell, currently documented on Qt 6.11.
- Its own .marathon apps are Qt/QML/Keel, launched by an isolated marathon-app-runner with bwrap, app identity and permissions; externally installed Wayland apps/Flatpaks use distinct launch and confinement paths.
- Upstream manifest schema defines manifestVersion, reverse-DNS id, entryPoint pointing to an existing QML file, icon, author, testedOn and dataLifecycle. Native binary-only .marathon is **not** assumed supported.
- Upstream shell IPC is org.marathonos.Shell over D-Bus; privileged APIs require permissions, security context and correct identity. Wayland is xdg-shell; compositor owns keyboard, gesture/navigation, lifecycle and scaling.
- Upstream developer's positive interest is an invitation to collaborate, not a merged interface or official compatibility certification. Nothing from this repository or its fork has been submitted to `patrickjquinn/Marathon-Shell` (checked 2026-11-19, see [UPSTREAM_RFC](UPSTREAM_RFC.md)).
- Re-read on 2026-11-19 at upstream `1f845160f0c249cb29da127fb66abaad380b5438`: the manifest schema still requires a `.qml` `entryPoint` and limits `toolkit` to `keel` or `marathonui`, and native `.desktop` apps still run unconfined.
- Authoritative references: https://github.com/patrickjquinn/Marathon-Shell/blob/main/README.md ; docs/APP_MANIFEST_SCHEMA.md ; docs/SECURITY_MODEL.md ; docs/NATIVE_APP_LIFECYCLE.md ; docs/WAYLAND_PROTOCOLS.md ; docs/PUBLISHING_GUIDE.md . Recheck against the current revision before coding.

## Target layout

```text
JS / React Native / Expo (shared application source)
       |                   |
  Metro --platform qnx    Metro --platform marathon
       |                   |
       +-- Hermes / JSI / Fabric / Yoga --+
                       |
    shared rnqnx_views_qt / fabric_adapter / react_host
                 |                 |
       QNX Qt QPA/Screen    Linux Qt Wayland client
                 |                 |
         QNX SDP runtime    MarathonOS compositor
                                  |
                       Permissioned shell adapters
```

## Nonnegotiable boundaries

1. One React Native Qt Quick component renderer; two display/platform adapters. No QNX Screen calls in portable renderer; no Marathon D-Bus calls in Fabric core.
2. Never claim QNX and Marathon share executable binaries. They share JS and portable source; C++ must compile separately against different libc, platform APIs and dependencies.
3. Stable JS API should expose distinct `Platform.OS` where practical; keep QNX behavior unchanged. For interim compatibility, an explicit qnx bundle alias must be labeled transitional, not called native Marathon support.
4. Marathon's special app metadata, Wayland app_id, permissions and lifecycle must be coordinated with upstream. A desktop .desktop or Flatpak launching on Wayland is milestone 1, not equivalent to a store-ready .marathon app.
5. Safe areas, physical/soft keyboard, keyboard insets, back, status/gesture bars, touch, lifecycle, background suspension and notification permissions each require device evidence.
6. Nitro 1.0 is alpha-nightly: the repository pins `1.0.0-alpha.nightly.136.gdb81de61bd32`, and npm `latest` is `0.37.2` (checked 2026-11-19). Expo Modules **API 2.0 beta** is exposed within Expo SDK 58; this is not an npm `expo-modules-core@2.x` request. Model C++/JSI interoperability separately from Swift/Kotlin-specific macros.
7. Dependencies follow the matched Expo 58 / RN 0.88 stable line (`expo` 58.0.24, `react-native` 0.88.1 in the lockfile), not an unconstrained latest-of-everything grab. Lockfile, headers, Hermes source, Metro, Expo internals, CMake and autolinking must move together.

## Milestones / exit gates

| Gate               | Requirements                                                                                                           | Evidence required                                            | Status 2026-11-19                                                                                                                                                                                                                                                                                                                                           |
| ------------------ | ---------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| M0 audit           | local reproducible host dependencies and authoritative current upstream interfaces                                     | dated compatibility matrix + source references               | Done: upstream re-read at `1f845160`, ledger in docs/validation                                                                                                                                                                                                                                                                                             |
| M1 bundle          | Marathon Metro production JS bundle, Platform.OS and resolver test; QNX bundle remains identical                       | bundle hashes, tests                                         | CI-verified: both bundles build and are hashed. CI does not diff the QNX bundle against a pre-change baseline                                                                                                                                                                                                                                               |
| M2 Wayland host    | Qt/JSI/Fabric renders a RN view into an xdg-shell window with touch and text; no QNX-specific libs                     | Linux native build, window screenshot, logs                  | Partial: host builds, renders offscreen, and creates the expected `xdg_toplevel` on a generic headless Weston in CI. In an Alpine VM on 2026-11-20 it ran against the real Marathon-Shell `1f845160`: window shown, pointer, drag scroll and text reached JS ([evidence](../evidence/marathon-vm-2026-11-20/README.md)). Touch and wheel not run; no device |
| M3 app shell       | foreground/background, back/forward, IME, permission-denied, rotation, scale, safe areas                               | device/simulator video, log tests                            | Partial, VM only: back (handled and unhandled), relaunch and exit ran against Marathon-Shell. IME, permission, rotation, safe areas not run; no shell contract exists for status bar, keyboard inset or open-URL ([SHELL_CONTRACTS](SHELL_CONTRACTS.md))                                                                                                    |
| M4 modules         | explicit Nitro v1 alpha and Expo API v2 beta capability matrix, no fabricated "works" rows                             | C++ symbols, generate/build/runtime logs                     | Partial: Marathon Nitro adapter is linked into the host and read from JS at runtime in CI (`--nitro-proof`, Qt dispatcher tests pass). Expo API v2 rows are open                                                                                                                                                                                            |
| M5 package         | confirmed upstream runner support for native executable or reviewed helper, signed .marathon installer and permissions | valid manifest lint, install/launch/uninstall logs           | Blocked on upstream: the schema accepts only QML entry points                                                                                                                                                                                                                                                                                               |
| M6 physical device | HackberryPi CM5 and one phone, QNX Pi separately                                                                       | distinct trace logs, screenshots, 3 clean launch/exit cycles | Not run. QNX Pi 400 runs exist and count only for QNX                                                                                                                                                                                                                                                                                                       |

## Performance and security acceptance

Measure cold/warm start, first contentful frame, 60fps/120fps where panel supports, dropped frames, input latency, per-process RSS/PSS, suspend/resume retention, thermal and crash behavior; targets are hypotheses until measured. No custom frame scheduler fighting Marathon compositor frame callbacks. Denied permission fails closed. No root access, bypass of bubblewrap, fake shell identity or unrestricted system bus. Source/binaries and any Qt modules must pass MIT/Qt licensing review.

## Verified in CI

Runs at PR #149 head `cbe27bf7` (merged 2026-11-19), all successful:

- [marathon-qt-host.yml run 44492351468](https://github.com/mikevocalz/react-native-qnx/actions/runs/44492351468): builds `rnqnx_marathon_host` with Qt 6.11.1, Hermes and Fabric on Ubuntu 24.04, renders one offscreen frame, and checks that touching `qnx-asset.png` rebuilds the Marathon bundle.
- [marathon-metro.yml run 44492351488](https://github.com/mikevocalz/react-native-qnx/actions/runs/44492351488): builds the QNX and Marathon production bundles and hashes both.
- [nitro-v1-host.yml run 44492351495](https://github.com/mikevocalz/react-native-qnx/actions/runs/44492351495): compiles the shared Nitro v1 core plus the QNX and Marathon adapters. Compile only, no runtime.

## Open items needing owner or upstream

| Item                                                                            | Who                                                                                                                                       | Command or action that produces the evidence                                                                                                                                            |
| ------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Run the host in a real Wayland compositor and capture the `xdg_toplevel` app_id | Done 2026-11-20 in an Alpine VM against Marathon-Shell `1f845160` (see the evidence directory); nothing further needed for the VM         | `WAYLAND_DISPLAY=<socket> QT_QPA_PLATFORM=wayland ./build/marathon-host/bin/rnqnx_marathon_host`, then `pnpm qnx marathon run --runner ... --bundle ...`; save logs and a screenshot    |
| Run on actual MarathonOS (QEMU AArch64 image or HackberryPi CM5)                | Owner with a device or VM image                                                                                                           | Same launch under the Marathon compositor; record scale, touch, keyboard, back, 3 launch/exit cycles                                                                                    |
| Build against the device libc and Qt (Alpine/musl, Qt 6.11)                     | Partly done: Alpine 3.24.4 musl, Qt 6.11.1, `file` and `ldd` recorded in the VM evidence. Owner: confirm against an actual Marathon image | Compare `ldd` and `qmake6 -v` from the image with `environment.txt` in the evidence directory                                                                                           |
| Native executable entry in the Marathon manifest and runner                     | Upstream maintainer (patrickjquinn)                                                                                                       | Nothing submitted upstream. The fork RFC (mikevocalz/Marathon-Shell PR #1, draft, open) is the only artifact. Owner decides whether to open an upstream discussion                      |
| Marathon status bar, insets, keyboard inset, open-URL                           | Upstream (no contract in the shell today)                                                                                                 | See [SHELL_CONTRACTS](SHELL_CONTRACTS.md): the shell source records `set_system_bars`, always sends zero insets and `ime_inset`, and has no http handler. Back and exit are implemented |
| Stock Pi 4 QSTI board and QNX VM for the QNX side                               | Owner                                                                                                                                     | `docs/evidence` receipts exist for the custom Pi 400 only                                                                                                                               |
| Performance numbers (start, frame time, RSS)                                    | Owner, device                                                                                                                             | Measure on device; no figure in this repository is a Marathon measurement                                                                                                               |
