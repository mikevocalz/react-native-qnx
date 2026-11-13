# Phase 6: everyday Expo developer experience and distributable tooling

Status: proposed implementation plan; documentation only.
Source baseline: main `cfc28165525e0d7c54192daf58b9b4318a26e83a`, 2026-11-07.
QNX developer acceptance consumes Phases1-4 ([external app](https://github.com/mikevocalz/react-native-qnx/pull/82), [modules](https://github.com/mikevocalz/react-native-qnx/pull/83), [UI](https://github.com/mikevocalz/react-native-qnx/pull/84), [services](https://github.com/mikevocalz/react-native-qnx/pull/85)). The Marathon variant additionally consumes the relevant [Phase5](https://github.com/mikevocalz/react-native-qnx/pull/86) runner/packaging contract. Diagnostics and package audits may start early.

## Outcome and existing groundwork

A new developer creates or adopts an ordinary Expo app, selects a native target, develops with useful reload/errors/debugging and produces a versioned installable application through one documented workflow. The consumer must not clone this platform monorepo or copy private examples. Retain React Native QNX branding and QNX as default; supported existing mobile/web app targets are not replaced.

Reuse [CLI](../../packages/react-native-qnx-cli/), [developer guide](../getting-started/qnx-cli.md), [package policy](../packaging.md), device tooling [PR78](https://github.com/mikevocalz/react-native-qnx/pull/78), preparation [PR79](https://github.com/mikevocalz/react-native-qnx/pull/79) and integrated fresh-source [PR80](https://github.com/mikevocalz/react-native-qnx/pull/80). Those are useful prerequisites, not already-published standalone tooling or inspector support. Preserve their error/overwrite/host-key safeguards.

## User-facing contract

One target-aware interface covers project diagnostics, prepare, bundle, native build, device install, launch, logs, package and development attachment. Select the final grammar after testing real consumer flows; examples in future implementation PRs must clearly distinguish supported commands from proposals. QNX is default and never silently redirected to a Linux host.

Diagnostics identify the failing step, exact incompatible package/ABI/toolchain, missing SDK/Qt/plugin/device capability and a verified recovery action. Avoid generic green checks from file existence alone. Failed children and interrupted commands preserve nonzero status; dry-run performs no device/network/write actions unless explicitly documented.

The dev runtime supports the implemented Metro/bundle transport, source maps, meaningful JS/native errors, reload and actual Fast Refresh semantics. Verify component state preservation versus intentional full reload, native module teardown and reconnect. A connected SSH tunnel is only transport; debugger acceptance requires discovering the real endpoint, attach, breakpoint, source mapping, resume, detach and clean reload. Do not advertise Hermes CDP or React DevTools readiness from an open port.

Stock Expo Go and EAS support are not assumed. Build the target-specific development runtime and declare its native module set. Signed release delivery and any update mechanism must remain separate from permissive development transport. No unreviewed remote code/chunk download path is introduced for convenience.

## Roster and path ownership

Actual human assignments remain unassigned; each implementation PR names its DRI and independent reviewer.

- CLI/DX owner: `packages/react-native-qnx-cli/`, integrated `packages/qnx-devtools/` and `packages/expo-qnx/`; reviewer: independent tooling engineer. Deliverable: one coherent consumer interface, bounded execution and safe recovery.
- Metro/development-runtime engineer: resolver packages, host bundle transport, errors/reload and source maps; reviewer: Hermes/Fabric owner. Deliverable: reproducible edit/reload loop with native cleanup.
- Inspector engineer: actual Hermes/inspector integration and dev-only transport; reviewer: security/JSI lifetime engineer. Deliverable: real attach/breakpoint/map/detach behavior, not tunnel-only tests.
- Build/release engineer: package manifests/exports/files, native toolchain metadata, local tarball and release scripts; reviewer: dependency/ABI reviewer. Deliverable: installable consumer artifacts with complete native/generated content.
- Licensing/supply-chain reviewer: Qt/plugin/module source notices, SDK exclusions and artifact provenance; independent reviewer: release/security owner. Deliverable: exact artifact inventory and no unauthorized distribution.
- Documentation/DX QA engineer: versioned getting-started, API/support limits and troubleshooting; reviewer: fresh developer not implementing the feature. Deliverable: literal clean-host reproduction.
- Target QA owner: QNX and Marathon installation/debug/upgrade matrices; reviewer: independent platform owner. Deliverable: separate target results and preserved mobile/web compatibility.

Proposed inspector/template/consumer-test paths are reserved only after actual source audit. Shared runtime/workflow changes remain integrator-sequenced. No roster entry claims vendor participation.

## Skills and decisions

Read repository `qnx-platform-validation`, `qnx-native-api` and the [v1 policy](../platform-modernization/NITRO_V1_API_POLICY.md). Native API/JSI lifetime slices use Margelo `api-design` -> `build-nitro-modules` -> `cpp` at `1e9e17be6f41c838db472d6a4b943fc26355a3be`, under `skills/<name>/SKILL.md`. Decisions: explicit public semantics, supported generated contracts and teardown-safe debugger/runtime ownership. No Nitro wrapper is added merely to rename existing debugger protocols.

Callstack `create-react-native-library`, `github-actions`, `react-native-best-practices` and `writing-user-docs`, source `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`, govern packed-file boundaries, exact-run artifacts, measured dev overhead and task-oriented verified instructions. Expo `expo-module`, source `a9e5bd3fa00b4e61f632e43f85da73aee1942582`, path `plugins/expo/skills/expo-module/SKILL.md`, governs explicit native support; audit exact installed Expo development-runtime internals separately.

Discover and pin upgrade/migration skills before an upgrade, and trusted code-review/design-quality skills (including the owner's no-ai-slop or everything-claude-code sources where applicable) before claiming their use. Their names alone are not installation/review evidence. No automatic scaffold/library migration or mobile-only device tooling is assumed to support QNX.

Source-read guidance is applied to planning; installation and native execution are not claimed. Implementation records source SHA/path, local install/read status, decision, command and artifact. The actual matched Nitro v1 contract overrides generic old samples.

## Implementation sequence

1. Consolidate existing commands around the Phase1 consumer project/target descriptor, retaining useful diagnostics and failure propagation. Test invalid configuration, stale cache, unsupported module and wrong-device cases.
2. Implement the real target development runtime and reload path. Verify source maps/errors and teardown under code changes, syntax error recovery, disconnect/reconnect and native module replacement.
3. Implement real debugger attachment using audited runtime interfaces. Keep debug endpoints local/authenticated or explicitly secured, development-only and absent/disabled in release builds. Test unauthorized attachment and competing sessions.
4. Pack and install exact-version artifacts into an isolated external app. Audit files/exports/headers/generated code/native plugin closure, reproducible manifests, compiler/runtime compatibility and install/update/remove. Start with local artifacts; publishing requires verified account/package ownership and release authorization.
5. Produce one recommended QNX guide and target-specific Marathon additions. Walk it on a clean supported host with licensed prerequisites, then actual named devices. Preserve consumer source and data on upgrade/reprepare.

## Acceptance and evidence

- [ ] Consumer uses versioned artifacts outside the monorepo; no hidden checkout paths.
- [ ] One documented create/adopt/build/install/run workflow with QNX default.
- [x] Reload, real Fast Refresh scope and errors behave as documented: the QNX development client ([dev workflow](../getting-started/dev-workflow.md)) on a Pi 400, 2026-11-13. Source maps: Metro serves them, but LogBox cannot symbolicate on device because `fetch` (RCTNetworking) is not implemented on QNX yet.
- [ ] Actual debugger endpoint attach/breakpoint/resume/detach and security tests pass.
- [ ] Native teardown/reconnect leaves no stale callbacks or orphan target processes.
- [ ] Package contents, ABI, notices and SDK/premium exclusions are reviewed.
- [ ] Fresh developer reproduces clean-host and target workflow; QNX/Marathon results are distinct.

Run existing frozen-install/typecheck/JS/native gates and add real packed-consumer and debugger tests. Record source commit, package/lock/binary/bundle identities, exact host/target, flags, commands, logs and artifact hashes. Prior PR80 source-directory evidence is not a clean-machine toolchain or device pass. Measure reload/attach/startup/memory overhead without inventing budgets or relabeling queued frames.

## Security, documentation, limits and rollback

No SSH host-key bypass, unsigned release-code fetch, public debugger exposure, secret upload or inherited arbitrary shell command. Preserve native app identity and per-app caches; debugger credentials stay out of artifacts. Follow ADR0013 eligible public-source Qt qualification and separate QNX SDK terms; no evaluation SDK payloads or Forge binaries in core packages. Root MIT licensing does not cover dependencies automatically.

Authoritative interface references for implementation audit: [RN out-of-tree platforms](https://reactnative.dev/docs/out-of-tree-platforms), [Expo autolinking](https://docs.expo.dev/modules/autolinking/) and [Expo custom native code](https://docs.expo.dev/workflow/customizing/). These establish extension boundaries, not official support for this target. Consumer UX decisions require exact observed command/source references and failure behavior, not marketing comparisons.

This plan does not publish packages or implement dev features. Rollback uses versioned compatible tooling/runtime artifacts, preserves app data and disables the new debug path. Never solve a package regression by silently upgrading only one Nitro/Expo dependency. Reverting this document alone changes no runtime. Phase7 remains the final release gate, with QNX qualification independent of Marathon's label.
