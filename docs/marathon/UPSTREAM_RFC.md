# Upstream RFC to MarathonOS — native React Native apps via existing Qt/Fabric host

**Discussion draft:** not yet submitted upstream. Intended audience: MarathonOS application runtime/runner maintainers and react-native-qnx maintainers.

## Request

Support separately compiled Linux/Wayland Qt Quick + Hermes + React Native Fabric applications, using the existing `react-native-qnx` Qt renderer, with first-class MarathonOS app identity, permissions, lifecycle, developer tooling and signed package installation. No Android container, WebView, second renderer or QNX executable emulation.

## Current upstream constraints

1. Marathon's documented .marathon app contract includes a QML `entryPoint`, `toolkit` selector (keel/marathonui), bubblewrap sandbox and `marathon-app-runner` lifecycle interface. Native Wayland apps are a separate launch class and ordinary distro binaries are not inherently app-sandboxed.
2. We need an upstream-reviewed entry mode for a **native Linux executable**, not an invented manifest field deployed into production.
3. Proposed interface (illustrative ONLY, not a valid manifest until schema/runner accepts it): `runtime: "native-wayland"`, `executable: "bin/example"`, `appId`, XDG data/cache roots, trusted permission mediation, keyboard/IME/back, scene scale and activation.
4. Preserve existing `manifestVersion:1`, signature verification, reverse-DNS identity, permission prompt and uninstall data semantics. Decide with maintainers whether to extend schema version, make a companion launcher, or use a Flatpak-first delivery path.

## Questions for Marathon maintainer

- Can `marathon-app-runner` launch a packaged Qt Quick native executable as an isolated process while maintaining its security-context Wayland client identity, bubblewrap and DBus per-app permission mediation? If not, which safe new runner path should be implemented?
- Which APIs should native clients use to receive app back/forward, suspend/resume/freeze, deep links, share targets, keyboard insets, gestures, notifications, and background tasks?
- Which Qt/Wayland modules and ABI are guaranteed on supported devices? Are Alpine/musl and ARM64 requirements stable, and are shared Qt plugins available inside bwrap?
- Which manifest version and signature/package validation should be used for a binary with no QML entrypoint?
- Which hardware target and simulator image are preferred for testing? Proposed: QEMU AArch64, HackberryPi CM5; verify with maintainer.
- What naming/credit, distribution, compatibility and CI evidence would justify labeling RN as officially supported out of the box?

## Acceptance proposal

A signed, installable example has visible first frame, native touch, keyboard, back, suspend/resume and per-app denied permissions on two Marathon targets, while QNX tests remain green independently. First upstream PR should be a narrowly scoped schema/runner/permission proof; no privileged bypass or unconfined workaround.

## Non-goals

No promise to execute QNX ELF binaries on Linux. No automatic port of arbitrary native Android/iOS libraries. No automatic support for all Expo/Nitro APIs or zero-code platform overrides. No security-context opt-out or release declaration until upstream merges tests.
