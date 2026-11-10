# PR 106: public Image and Switch-style review

Reviewed 2026-11-10 against implementation commit
`6b528c17967177f92b5894565fb0b93eac226dd2`, based on
`0c91be9bbb4e7cd6dabccf8cdfe441c7fa57c629`.
This follow-up updates the candidate's validation and required handoff records.
It changes no runtime, test, dependency, native binding or release gate.

## Behavior retained

The public QNX Image wrapper delegates its size, header-aware size, prefetch,
metadata-prefetch and cache methods to the existing native ImageLoader using
the installed RN 0.88.1 contract. The corrected `queryCache` implementation
retains fresh-Promise identity, immediate native invocation, synchronous-error
rejection, thenable adoption and returned-object identity without adding a
Babel runtime dependency. The Switch wrapper composes its intrinsic default
style and caller style as a React Native style array.

The original async syntax caused a real Metro helper-resolution failure. That
failure was repaired in `6b528c1`; the regression uses an isolated temporary
cache plus `--reset-cache`. Earlier failed runs remain historical evidence.

## Local verification

Linux x86-64, Node 26.13.0, pnpm 12.8.1; installed dependencies match the frozen
lock SHA-256
`b854ebe2e50a42b2e8e07124df065592d99d43b1d1a3a984b66d3835ad86c3e4`.
React Native remains 0.88.1, Hermes `hermes-v260318099.0.5` and Nitro
core/adapter/generator `1.0.0-alpha.nightly.105.g69d365fc9b18`.

| Command                                                                                    | Actual result                                                                                                                                                                                          |
| ------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `node tests/js/qnx-public-entry.test.mjs`                                                  | Passed: public exports/resolver, Switch nested style precedence/events/props and Image Promise/callback/error/loader contracts. Controlled native loader and React VM doubles; no native decode claim. |
| `node tests/js/qnx-public-entry-metro.test.mjs`                                            | Passed using real Metro with isolated reset caches: QNX 684,415 bytes; Marathon 684,456 bytes. Bundling is separate from runtime execution.                                                            |
| `pnpm typecheck`, `pnpm validate:dependencies`                                             | Passed.                                                                                                                                                                                                |
| `pnpm docs:sync`, `pnpm validate:docs`, changed-file Prettier check and `git diff --check` | Passed on the follow-up documentation tree.                                                                                                                                                            |
| Full local JS / native / ASan suites                                                       | Not rerun for this documentation-only follow-up. The original author's full-JS attempt encountered the existing Marathon Unix-socket restriction.                                                      |
| QNX cross-build, VM, physical QNX and Marathon shell                                       | Not run for this candidate.                                                                                                                                                                            |

## Hosted evidence at the repaired implementation head

The GitHub API snapshot checked on 2026-11-10 reports successful completion at
`6b528c1` for:

- [Public imports native host](https://github.com/mikevocalz/react-native-qnx/actions/runs/42652106074):
  the inspected job passed native build/regressions and public-bundle rendering.
- [SDK 58 full host renderer](https://github.com/mikevocalz/react-native-qnx/actions/runs/42652105343).
- [SDK 58 validation](https://github.com/mikevocalz/react-native-qnx/actions/runs/42652105366).
- [Marathon Metro conformance](https://github.com/mikevocalz/react-native-qnx/actions/runs/42652105673).
- [Image loader HTTP/cache](https://github.com/mikevocalz/react-native-qnx/actions/runs/42652105688).

These runs qualify their named implementation revision on their host lanes.
They do not establish physical QNX/Marathon acceptance or CI for a later
follow-up/integration commit. No human approval was recorded in that snapshot;
the remaining inline review asked for the six committed handoff records.

## Combining this branch with PR 110

Neither `6b528c1` nor its repaired Image API is in PR 110's inspected
`083c4bdb887a844e533313fed9b967ff94f4c4d3` tree. The owner's PR 110 smoke test
therefore cannot be assigned to this change.

Retain both this branch's style-array composition and PR 110's controlled
Switch reconciliation. Its new hook-based wrapper adds `useMergeRefs`,
`useRef`, `useState` and `useLayoutEffect`. This branch's isolated Switch
mock only implements `forwardRef` and `createElement` and assumes the native
ref equals the caller's ref; those assumptions no longer hold after integration.

The combined branch should put the style/prop assertions into PR 110's existing
`qnx-controlled-switch.test.mjs` retained-hook harness, exposing the latest
rendered props for those assertions. Keep the harness's accepted/rejected,
deferred, disabled and ref-cleanup checks. Keep Image/public-entry/resolver
assertions in `qnx-public-entry.test.mjs` and preserve PR 110's added
`I18nManager` export assertion. This is integration work, not a change made
by this documentation-only follow-up.

## Skills, ownership and next action

| Source read                                          | Pin                                        | Applied decision                                                                                     |
| ---------------------------------------------------- | ------------------------------------------ | ---------------------------------------------------------------------------------------------------- |
| Repository QNX Native API / QNX Platform Validation  | Sources at the implementation revision     | Keep JS-wrapper, Metro, host-native and device evidence separate; preserve current native contracts. |
| Margelo `api-design` → `build-nitro-modules` → `cpp` | `1e9e17be6f41c838db472d6a4b943fc26355a3be` | Preserve caller-visible Promise/error and v1 ownership contracts; no new handwritten native API.     |
| Callstack `writing-user-docs`                        | `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` | Update the developer guide with exact hosted evidence and visible limitations.                       |

These source files were read; no personal skill installation is claimed.
Handoff DRI: the assigned public-API review agent. Independent integration
reviewer: the parent integrator; acceptance is recorded separately. Human
device-qualification owner remains unassigned.

Publish and review the follow-up's own checks, then resolve the handoff thread.
Validate the combined Switch/Image branch with both focused tests, both reset
Metro bundles and the normal native-host checks before target qualification.
All seven v1 gates remain unchanged.

This follow-up has no runtime performance, privilege, licensing, premium
dependency or persistent-data effect. Reverting it removes documentation;
reverting the complete PR restores the earlier public wrapper behavior.
