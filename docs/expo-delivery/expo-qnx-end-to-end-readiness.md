# Expo SDK 58 → QNX: end-to-end acceptance

This is the umbrella **implementation and acceptance** track for React Native QNX Expo readiness. It does not turn existing proposed PRs, host simulations or planning documents into claimed native device support.

## Current boundary

- Expo SDK 58 and React Native 0.88 / Hermes / Fabric / Yoga / Qt Quick are pinned in the repository.
- External creation and native-build candidate work exists in PRs [#100](https://github.com/mikevocalz/react-native-qnx/pull/100) and [#101](https://github.com/mikevocalz/react-native-qnx/pull/101).
- Expo discovery, JSI/bootstrap and target-specific autolinking have earlier slices in [#11](https://github.com/mikevocalz/react-native-qnx/pull/11), [#39](https://github.com/mikevocalz/react-native-qnx/pull/39), [#79](https://github.com/mikevocalz/react-native-qnx/pull/79) and [#90](https://github.com/mikevocalz/react-native-qnx/pull/90).
- Adaptive input/window and development host changes are being integrated in [#104](https://github.com/mikevocalz/react-native-qnx/pull/104), [#108](https://github.com/mikevocalz/react-native-qnx/pull/108) and [#110](https://github.com/mikevocalz/react-native-qnx/pull/110).
- Design and implementation plans already live in [phases 0–7](README.md), including Router/gestures, platform services, developer loop and release evidence. **Plan PRs are not functionality.**
- QNX, host Linux and MarathonOS require separate receipts. A host-preview bundle is never device qualification.

## Run the readiness inventory

From the source checkout:

```sh
pnpm validate:expo:qnx
pnpm validate:expo:qnx:strict
# Only when qualifying the separately installed premium consumer:
node scripts/validate-expo-qnx-readiness.mjs --strict --include-optional
```

The default command validates the manifest, prints pending/passed gates, and exits successfully when its **structure** is valid. Strict mode fails until all nine core gates have evidence. The tenth gate, Nitro Forge consumer compatibility, is optional and never blocks open-core readiness. Add `--include-optional` with `--strict` only when you also want to require that consumer proof. The inventory must retain all ten gate IDs; their required/optional classification cannot be changed in the manifest. The existing seven release gates remain separately authoritative via `pnpm validate:v1`. Never promote status merely because a file exists: a reviewer must inspect its actual QNX results, hashes, flags, board/image and test scope.

For each gate in [qnx-readiness-gates.json](qnx-readiness-gates.json), implement and test the named native capability, commit a revision-bound evidence record, independently review the underlying claim, then set `status: "passed"` and `evidence` to a non-symlink relative repository file. Evidence files must be nonempty and repository-relative, with no symlink in the path. Pending gates must retain `null` evidence. The checker does not establish Git tracking, receipt authenticity or hardware execution; reviewers must confirm the record is committed at the reviewed revision. Invalid manifests, missing evidence and pending strict gates exit 1; invalid CLI arguments exit 2. You can run the command from any working directory.

Run `node tests/js/expo-qnx-readiness.test.mjs` for isolated inventory, optional-gate, path-safety and CLI regression tests. The same test runs in `pnpm test:js` and the existing SDK 58 validation workflow. All gates in this PR remain pending; passing the structural tests is not QNX runtime acceptance.

## Required reference app and testing sequence

Use **one** external Expo SDK 58 app with public React Native / Expo imports, Zustand state and no required premium UI packages. Do not duplicate screens between QNX and Marathon to disguise platform gaps. The same app should exercise:

1. Project creation, install, Metro bundle, Expo discovery, real native autolink/JSI call, compiled binary, asset hashes and launch on a documented QNX target.
2. Expo Router stack, tab, modal, deep link, Back and route restoration with real Qt/Fabric event dispatch.
3. Scrollable virtualized list, controlled input, focus, keyboard/IME, safe areas, permissions and accessibility actions.
4. Gesture Handler recognizers, Worklets/Reanimated interruptible drawer, mount/unmount and native teardown; distinguish native animation from Reanimated support.
5. Fast Refresh, stale/invalid bundle recovery, Metro reconnection and state preservation; never use reload success as proof of Fast Refresh.
6. Platform services (Camera, Sensors, Notifications, storage and lifecycle) each with an implementation-or-explicit-unsupported decision. Do not map an Android/Apple implementation to QNX by naming alone.
7. Separately installed Nitro Forge module smoke tests with pinned Nitro ABI; the core runtime must still pass without premium packages.
8. Qt library/plugin closure, licenses, clean-machine provisioning, deploy/relaunch/rollback, twenty supervised graphics cold boots and device performance receipts.

## Integration and merge order

Preserve upstream branch ancestry and source ownership. First integrate the external consumer and build paths (#100/#101) against the latest base; then the input/adaptive/development host stack (#104 → #110 → #108, taking #106 into account). Layer real native module and Router/gesture/service implementations on top only after resolving the shared runtime/Qt/CMake ownership. Do not merge phase plans as evidence of functionality or squash away provenance without preserving references.

**Target acceptance** needs the exact board and OS image, Qt/QPA version and licenses, executable/HBC/assets hashes, relevant build flags, physical input mode, logs/screenshots where applicable, pass/fail matrix, reviewer, negative controls and a rollback path. Keep stock Pi 4 QSTI and custom Pi 400 results separate.

## Explicit exclusions

This PR adds a machine-checkable Expo acceptance inventory and guidance; it does **not** implement Router, Reanimated, RNGH, missing Expo modules, physical deployment, or distribution, and it does not assert hardware testing. Those stay pending until integrated code and named-target proof are reviewed. The repo's v1 release gates are unchanged.
