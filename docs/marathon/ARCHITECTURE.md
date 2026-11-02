# React Native QNX + MarathonOS — Architecture and proof boundaries

Status: design approved for experimental implementation proposals, **not a claim of shipped MarathonOS support**. Last reviewed 2026-11-02.

## Existing repository evidence

- `rnqnx_views_qt`: Qt Quick view/item factory and image provider.
- `rnqnx_fabric_adapter`: Fabric shadow-view mutation adapter, touch, scrolling and controls.
- `rnqnx_react_host`: Hermes JSI + runtime scheduler + React/Fabric host over Qt.
- `apps/rn-playground`: production Metro QNX bundle and host rendering fixture.
- `packages/metro-config-qnx`, `packages/react-native-qnx`: QNX Metro resolver and platform JS overrides.
- `packages/qnx-nitro-runtime`: existing synchronous QNX Nitro integration.
- `packages/expo-modules-core-qnx`, `expo-qnx-autolinking`: QNX Expo modules bootstrap/autolinking.
- `native/platform/linux-host`: existing directory, currently only scaffold.
- QNX native Qt runtime: implementation graph exists, full target execution remains unproven; QNX SDP 8.0.5 Pi/QSTI run pending. Host Qt renderer results are not QNX device results.

## Marathon upstream evidence (patrickjquinn/Marathon-Shell)

- Marathon is a Linux/postmarketOS mobile OS with Qt 6 Wayland compositor and app shell, currently documented on Qt 6.11.
- Its own .marathon apps are Qt/QML/Keel, launched by an isolated marathon-app-runner with bwrap, app identity and permissions; externally installed Wayland apps/Flatpaks use distinct launch and confinement paths.
- Upstream manifest schema defines manifestVersion, reverse-DNS id, entryPoint pointing to an existing QML file, icon, author, testedOn and dataLifecycle. Native binary-only .marathon is **not** assumed supported.
- Upstream shell IPC is org.marathonos.Shell over D-Bus; privileged APIs require permissions, security context and correct identity. Wayland is xdg-shell; compositor owns keyboard, gesture/navigation, lifecycle and scaling.
- Upstream developer's positive interest is an invitation to collaborate, not a merged interface or official compatibility certification.
- Authoritative references: https://github.com/patrickjquinn/Marathon-Shell/blob/main/README.md ; docs/APP_MANIFEST_SCHEMA.md ; docs/SECURITY_MODEL.md ; docs/NATIVE_APP_LIFECYCLE.md ; docs/WAYLAND_PROTOCOLS.md ; docs/PUBLISHING_GUIDE.md . Recheck against current revision before coding.

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
6. Nitro 1.0 is alpha-nightly as of 2026-11-02; stable npm latest remains 0.37.1. Expo Modules **API 2.0 beta** is exposed within Expo SDK 58; this is not an npm `expo-modules-core@2.x` request. Model C++/JSI interoperability separately from Swift/Kotlin-specific macros.
7. Dependencies follow the matched Expo 58 / RN 0.88 stable line, not an unconstrained latest-of-everything grab. Lockfile, headers, Hermes source, Metro, Expo internals, CMake and autolinking must move together.

## Milestones / exit gates

| Gate               | Requirements                                                                                                           | Evidence                                                     |
| ------------------ | ---------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------ |
| M0 audit           | local reproducible host dependencies and authoritative current upstream interfaces                                     | dated compatibility matrix + source references               |
| M1 bundle          | Marathon Metro production JS bundle, Platform.OS and resolver test; QNX bundle remains identical                       | bundle hashes, tests                                         |
| M2 Wayland host    | Qt/JSI/Fabric renders a RN view into an xdg-shell window with touch and text; no QNX-specific libs                     | Linux native build, window screenshot, logs                  |
| M3 app shell       | foreground/background, back/forward, IME, permission-denied, rotation, scale, safe areas                               | device/simulator video, log tests                            |
| M4 modules         | explicit Nitro v1 alpha and Expo API v2 beta capability matrix, no fabricated "works" rows                             | C++ symbols, generate/build/runtime logs                     |
| M5 package         | confirmed upstream runner support for native executable or reviewed helper, signed .marathon installer and permissions | valid manifest lint, install/launch/uninstall logs           |
| M6 physical device | HackberryPi CM5 and one phone, QNX Pi separately                                                                       | distinct trace logs, screenshots, 3 clean launch/exit cycles |

## Performance and security acceptance

Measure cold/warm start, first contentful frame, 60fps/120fps where panel supports, dropped frames, input latency, per-process RSS/PSS, suspend/resume retention, thermal and crash behavior; targets are hypotheses until measured. No custom frame scheduler fighting Marathon compositor frame callbacks. Denied permission fails closed. No root access, bypass of bubblewrap, fake shell identity or unrestricted system bus. Source/binaries and any Qt modules must pass MIT/Qt licensing review.
