# Reanimated, Worklets and RNGH: exact-source QNX audit

Audited November 7, 2026. This extends the [integration map](reanimated-gesture-handler-qnx-map.md).
It selects exact packages for port investigation and records concrete gaps.
It does not install them into the application, change a lockfile, implement a
port or claim that any of these libraries runs on QNX.

## Exact inputs and version decision

The QNX baseline is `e84b51430c239bc89dbd48ac35afab89f7c77c01`: RN 0.88.1,
React 19.3.0, Expo 58.0.22 and pinned Hermes 260318099.0.5. The later owned
Qt/Nitro runtime was separately inspected at
[`e9026fa`, PR #44](https://github.com/mikevocalz/react-native-qnx/pull/44).
Neither that proof nor Forge's native engine tests establish library support.

| Audit target     | Published constraints / source evidence                                             | Decision                                                    |
| ---------------- | ----------------------------------------------------------------------------------- | ----------------------------------------------------------- |
| Reanimated 4.7.1 | Peer RN `0.86 - 0.88`, Worklets `0.13.x`; shipped Fabric compatibility table agrees | Candidate for the pinned RN 0.88 baseline, pending QNX port |
| Worklets 0.13.1  | Peer RN `0.86 - 0.88`; shipped compatibility table agrees                           | Candidate patch release in the required 0.13 line           |
| RNGH 3.5.0       | Wildcard RN peer; package development baseline RN 0.88.0; README minimum 0.82       | Audit target, not proof of RN 0.88/QNX native compatibility |

These were the registry `latest` tags observed for this audit, not moving refs
to use in an implementation. A later implementation must pin exact versions
and regenerate its own dependency graph. The
[Reanimated compatibility table](https://docs.swmansion.com/react-native-reanimated/docs/guides/compatibility/)
lists the 4.7/0.13 pairing. The
[RNGH table](https://docs.swmansion.com/react-native-gesture-handler/docs/fundamentals/getting-started/)
currently lists 3.4.x with RN 0.88 support and 3.3.x without it; it does not yet
list the observed 3.5.0 release. Wildcard peers do not resolve that evidence gap.

Expo 58.0.22's own `bundledNativeModules.json` selects Reanimated 4.7.1,
Worklets 0.13.0 and RNGH `~3.3.0`. That installed file was compared byte-for-byte
with the integrity-verified
[published Expo 58.0.22 archive](https://registry.npmjs.org/expo/-/expo-58.0.22.tgz).
The RNGH choice conflicts with the upstream RN 0.88 compatibility table. Do not
blindly use `expo install` as proof of a compatible QNX combination. Record the
chosen departure from Expo's bundled versions and consumer API migration before
changing dependencies; no existing NYC-Tron/Forge consumer was audited here.

## Immutable source provenance

All three npm archives passed their registry SHA512 integrity checks. Each
listed key file's Git blob hash also matches its immutable release commit.
This is a targeted source comparison, not a claim that every generated npm file
matches a Git checkout. Archive hashes, peer ranges, Expo file hash and smoke
results are in the [machine-readable receipt](evidence/qnx-animation-source-audit-2026-11-07.json).

| Package          | Release commit                             | Key file compared                                                                                                                                                                                                                 |
| ---------------- | ------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Reanimated 4.7.1 | `5fd93c32624c64c4020fb089171afde88a511d92` | [PlatformDepMethodsHolder.h](https://github.com/software-mansion/react-native-reanimated/blob/5fd93c32624c64c4020fb089171afde88a511d92/packages/react-native-reanimated/Common/cpp/reanimated/Tools/PlatformDepMethodsHolder.h)   |
| Worklets 0.13.1  | `24e415d2c036a532cf7820ff0c400e49841bda50` | [UnpackerLoader.h](https://github.com/software-mansion/react-native-reanimated/blob/24e415d2c036a532cf7820ff0c400e49841bda50/packages/react-native-worklets/Common/cpp/worklets/SharedItems/UnpackerLoader.h)                     |
| RNGH 3.5.0       | `bbff42199d238724397ca94f4076246d8c02fb91` | [NativeRNGestureHandlerModule.ts](https://github.com/software-mansion/react-native-gesture-handler/blob/bbff42199d238724397ca94f4076246d8c02fb91/packages/react-native-gesture-handler/src/specs/NativeRNGestureHandlerModule.ts) |

All three package manifests declare MIT. No upstream source/binary is vendored
by this PR; implementation/distribution must preserve applicable notices.
Existing Qt evaluation, QNX licensing and Marathon confinement remain separate.

## Worklets: real runtime and lean-Hermes gates

`WorkletRuntime.cpp` creates its own Hermes runtime with microtasks and optional
locking/event-loop ownership. `UIScheduler` supplies a queue and a platform
`queryIsOnUIThread`; `RuntimeBindings` requires a frame callback, logging and
networking backend contract. `JSScheduler` requires an RN CallInvoker plus an
explicit JS-owner predicate. Initializer prepare/finalize/invalidate ordering
and runtime abort tokens must be preserved.

PR #44's dispatcher posts completions into the existing RN runtime and its RN
checkpoint. It is not a Worklets UI runtime, Worklets scheduler, frame clock,
runtime locking or cross-runtime value-transfer implementation. Reuse the host
ownership principles and RN completion route; implement the distinct upstream
interfaces rather than casting a Nitro dispatcher into them.

The Pi currently uses lean Hermes, which rejects source compilation. Worklets'
normal source unpackers and `evalWithSourceUrl` therefore need a different
delivery path. This is not an absolute Worklets/Hermes incompatibility:

- `UnpackerLoader.h` implements both source and bytecode unpackers. If both are
  loaded, source takes precedence. The port must select and prove the intended
  path, not merely load HBC alongside source.
- `NativeWorklets.native.ts` selects six bytecode unpackers when their init data
  contains bytecode; development builds explicitly reject that case.
- The published Babel plugin supports `hermesBytecode: true` and
  `getHBCBinary`, in release/non-Bundle Mode. Missing compiler or compilation
  failure falls back to source strings with a warning. That fallback is not
  acceptable for the lean target: detect it and fail the QNX build.
- Bundle Mode is another upstream path, with its own Metro serializer/resolver,
  entry-point interception and native bundle loader. It is not enabled by this
  audit; the current QNX Babel config contains only `babel-preset-expo`.

An isolated host smoke check loaded Worklets 0.13.1 with Babel 7.29.7 on
Node 26.13.0, using the pinned macOS Hermes compiler. This input:

```js
export function compute(value) {
  'worklet';
  return value + 1;
}
```

was transformed with production environment, `hermesBytecode: true` and
`getHBCBinary` returning the pinned compiler. The plugin emitted a 256-byte HBC
array with zero warnings. Missing transitive Babel plugins in the application
tool tree initially prevented loading; a separate 72-package audit installation
with lifecycle scripts disabled supplied the tool dependencies. The application's
manifest, lockfile, node_modules and Babel/Metro configuration were not changed.

This proves one plugin-to-HBC transformation only. It does not prove all six
unpackers, closure serialization, actual worklet execution, a Metro bundle,
Worklets C++ compilation or QNX runtime behavior. The private audit lock hash is
recorded; a real integration needs its own reviewed lockfile and fail-closed
bytecode checks. `scheduleOnRN`/`scheduleOnUI` are the inspected scheduling APIs;
do not copy obsolete application snippets as a substitute for source review.

## Reanimated: platform hooks are still missing

`PlatformDepMethodsHolder.h` defines `SynchronouslyUpdateUIPropsFunction` only
for Android or Apple and then uses that type unconditionally. There is no
generic QNX definition in this audited file. This is a source-level port gap,
not a compiler result: no QNX Reanimated build was attempted. Do not spoof
Android/Apple macros to select a foreign ABI.

The holder also needs rendering/timestamps, property updates, gesture state,
sensors, keyboard and pseudo-selector callbacks. `ReanimatedModuleProxy` takes
the separate UI worklet runtime, UI scheduler, RN runtime/CallInvoker and platform
methods, then registers Fabric commit/mount hooks and animation machinery.
Its Android CMake lane depends on ReactAndroid/fbjni and Android-specific source;
it cannot be reused as a generic QNX target.

The first port must define precise supported/unsupported behavior and an audited
QNX build target. An alias fix alone does not implement measurement, animated
prop updates, layout animation or frame scheduling. Qt item mutation must stay
on the GUI owner; Yoga remains the layout authority. A QTimer/native Animated
proof does not establish Reanimated timing or frame-budget conformance.

## RNGH: recognition and bootstrap are platform work

The archive contains Android Kotlin recognizers and Apple native implementations.
Shared C++ runtime decoration and shadow-node code do not supply a portable pan,
tap, pinch or cancellation state machine. Its Android CMake lane requires
ReactAndroid/fbjni. Qt needs a real registry/recognizer/input integration with
upstream state/threshold/composition behavior, native components and tag ownership.

`NativeRNGestureHandlerModule.ts` enforces a module named `RNGestureHandlerModule`;
it includes create/attach/configure/drop/flush and UI-runtime-binding operations.
Worklets and Reanimated also use native module bootstrap plus JSI proxies. The
current host has no provider for these library modules. Installing Nitro alone
does not satisfy their registries or native components.

[ADR-0004](adr/0004-nitro-modules.md) permits a minimal adapter for RN core's own
registry requirements and assigns new application APIs to Nitro. These
third-party bootstrap interfaces are outside that explicitly confirmed core
exception. Record the architecture decision before implementing them: preserve
actual upstream behavior/imports without empty TurboModules, a fake Nitro facade
or an unreviewed broadening of the native API policy.

The existing pointer seam also needs TouchCancel/grab-loss/window-deactivation,
real pressure/screen-coordinate semantics, synthesis deduplication and scroll
arbitration. First validate cancellation and target lifetime, then pan/shared
value behavior; do not run uncoordinated Qt and RNGH recognizers on one stream.
RNGH 3's hook API is the candidate line; consumer v2-to-v3 migration is a separate
audit. No application files or state management were changed.

## Ordered next implementation gates

1. Resolve the version/Expo and ADR bootstrap decisions in review; pin a real
   integration graph without changing the disabled QNX path.
2. Add a default-OFF Worklets target and owned UI scheduler/runtime. Prove six
   unpackers, bytecode-only release delivery, RN roundtrip and shutdown on host,
   then licensed QNX compilation and physical execution.
3. Add one Reanimated opacity/translation path through real Fabric/Qt hooks,
   with interruption, surface unmount and monotonic frame/timestamp tests.
4. Add RNGH registry/native components plus one pan recognizer, terminal/cancel
   delivery, transformed hit testing and nested-scroll arbitration.
5. Exercise a consumer using Zustand for React/document state and ephemeral
   animated values for preview; commit one Forge transaction after completion.
   Reload/suspend-resume, broader gestures and hardware performance stay gated.

## Validation, ownership and rollback

Executed: exact archive integrity checks; three key-file comparisons against
release commits; Expo file comparison; isolated Babel/HBC smoke; Markdown/link
and diff checks. No application dependency install, full Metro bundle, native
library/CMake/sanitizer build, QNX device or Marathon runtime test was run for
this audit. Earlier PRs #42/#44 results remain independent evidence.

DRI `/root`, platform/source integration audit. Independent reviewer
`/root/renderer_build_audit` confirmed version constraints, platform-only hooks,
the source-unpacker precedence and distinction from PR #44's RN dispatcher.
This is agent review, not upstream/maintainer approval.

Read guidance: Software Mansion React Native best practices, animations,
gestures and multithreading. Exact local SHA256 values and applied decisions
are recorded in the PR description. They directed separate runtime ownership,
version-specific APIs and explicit cross-runtime scheduling; no new HybridObject
or public TypeScript contract was designed. Native API skills remain required
when that implementation begins.

Rollback is reverting these documentation/evidence changes. No dependency,
runtime, display/window contract, graphics boot, permission or application-state
migration is needed. CI remains subject to the existing GitHub billing blocker.
