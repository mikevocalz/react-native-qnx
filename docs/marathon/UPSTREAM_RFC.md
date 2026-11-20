# Upstream RFC to MarathonOS - native React Native apps via existing Qt/Fabric host

**Discussion draft.** Intended audience: MarathonOS application runtime/runner maintainers and react-native-qnx maintainers.

## Current state (checked 2026-11-19, read-only)

- Nothing has been submitted to `patrickjquinn/Marathon-Shell`. `gh pr list -R patrickjquinn/Marathon-Shell --author mikevocalz --state all` returns no pull requests. Issue searches for "react native", "native" and "mikevocalz" find no issue about this work; the three hits for "react native" are unrelated CI and dependency issues (#58, #59, #60) by another author.
- The only RFC artifact is the fork PR [mikevocalz/Marathon-Shell#1](https://github.com/mikevocalz/Marathon-Shell/pull/1), "docs(plan): Marathon native client and managed runner contract". It is **open and a draft**, created 2026-11-07, last updated 2026-11-09, mergeable, with no comments or reviews. Its base is upstream `c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5` and its head is `209485b7d23c4468ab67c45348af5c3e7ef7df12`. The RFC text is committed at `d16e6db81f5bafde099c5452995f8f46bd05a51c` (`docs/rfcs/0013-react-native-client-contract.md`); the later commits on the branch port upstream CI and test fixes. Its three checks (`build-and-test`, `format-check`, `commit-subjects`) passed on 2026-11-09.
- The fork has 21 open PRs and no other RFC for the managed client. The other branches are unrelated fork work (the v2 line, a passport research branch).
- Upstream `main` was `1f845160f0c249cb29da127fb66abaad380b5438` (2026-11-19T18:05Z), 1023 commits after the RFC's base.

### Do the upstream constraints still hold

Yes. At `1f845160`:

- `docs/APP_MANIFEST_SCHEMA.md` (last changed in `863a6c6c`, 2026-11-15) still says `entryPoint` is "the main `.qml` file" and lists `toolkit` as `keel` or `marathonui`. `schemas/marathon-app-manifest-v1.schema.json` still requires `entryPoint` matching `\.qml$` and sets `additionalProperties: false`. A native executable is not a valid entry point.
- `docs/SECURITY_MODEL.md` (last changed in `27f7e1f3`, 2026-11-05) still lists native `.desktop` apps as plain processes with no confinement. Flatpak and `.marathon` apps are the confined routes.
- `docs/NATIVE_APP_LIFECYCLE.md` (last changed in `9cc1e334`, 2026-10-23) still documents back handling for native clients by a 350 ms heuristic and says a window that repaints continuously counts as handled.

The "QML `entryPoint` only" claim in this RFC and in the fork RFC stands.

## Request

Support separately compiled Linux/Wayland Qt Quick + Hermes + React Native Fabric applications, using the existing `react-native-qnx` Qt renderer, with first-class MarathonOS app identity, permissions, lifecycle, developer tooling and signed package installation. No Android container, WebView, second renderer or QNX executable emulation.

## Current upstream constraints

1. Marathon's documented .marathon app contract (verified against upstream `1f845160` on 2026-11-19) includes a QML `entryPoint`, `toolkit` selector (keel/marathonui), bubblewrap sandbox and `marathon-app-runner` lifecycle interface. Native Wayland apps are a separate launch class and ordinary distro binaries are not inherently app-sandboxed.
2. We need an upstream-reviewed entry mode for a **native Linux executable**, not an invented manifest field deployed into production.
3. Proposed interface (illustrative ONLY, not a valid manifest until schema/runner accepts it): `runtime: "native-wayland"`, `executable: "bin/example"`, `appId`, XDG data/cache roots, trusted permission mediation, keyboard/IME/back, scene scale and activation.
4. Preserve existing `manifestVersion:1`, signature verification, reverse-DNS identity, permission prompt and uninstall data semantics. Decide with maintainers whether to extend schema version, make a companion launcher, or use a Flatpak-first delivery path.

## Questions for Marathon maintainer

- Can `marathon-app-runner` launch a packaged Qt Quick native executable as an isolated process while maintaining its security-context Wayland client identity, bubblewrap and DBus per-app permission mediation? If not, which safe new runner path should be implemented?
- Which APIs should native clients use to receive app back/forward, suspend/resume/freeze, deep links, share targets, keyboard insets, gestures, notifications, and background tasks?
- Which Qt/Wayland modules and ABI are guaranteed on supported devices? Are Alpine/musl and ARM64 requirements stable, and are shared Qt plugins available inside bwrap?
- Which manifest version and signature/package validation should be used for a binary with no QML entrypoint?
- Which hardware target and simulator image are preferred for testing? Proposed: QEMU AArch64, HackberryPi CM5; verify with maintainer. No maintainer has been asked yet.
- What naming/credit, distribution, compatibility and CI evidence would justify labeling RN as officially supported out of the box?

## Acceptance proposal

A signed, installable example has visible first frame, native touch, keyboard, back, suspend/resume and per-app denied permissions on two Marathon targets, while QNX tests remain green independently. First upstream PR should be a narrowly scoped schema/runner/permission proof; no privileged bypass or unconfined workaround.

## Non-goals

No promise to execute QNX ELF binaries on Linux. No automatic port of arbitrary native Android/iOS libraries. No automatic support for all Expo/Nitro APIs or zero-code platform overrides. No security-context opt-out or release declaration until upstream merges tests.

## Owner action

Decide whether to turn fork PR #1 into an upstream discussion or issue. This repository does not open anything in the upstream organization. Until then the Marathon target stays a development client: a Wayland executable launched outside the app runner.
