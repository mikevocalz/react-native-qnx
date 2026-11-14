# PR 159: keyboard-hidden tab bar spacing

Base: `d862625e79d6b1dd8a589eaf767e3311757c449a`. Lockfile SHA-256:
`edf0327a216c8963c0ffea2300c37f0b7e29d05510e7f3fb5a3a2a4ad9feea49`.
This follow-up changes the QNX router adapter and its regression test. It changes
no dependency version, native API, renderer, window contract or release gate.

## Defect and change

The keyboard visibility gate removed the tab bar but never notified Expo
Router's `BottomTabView`. A scene reading the real bottom-tab-height context
therefore retained a 70 px inset after a 60 px bar plus 10 px safe inset was
hidden. The regression uses the installed Expo Router 58.0.31 navigator and its
actual context objects, with React Native host and animation test doubles.
It fails on the original adapter with `70 !== 0`.

The adapter now reports its occupied height through the upstream
`BottomTabBarHeightCallbackContext`. It reports zero once hiding finishes and
restores the height when showing. Explicit hidden state, rail position, height
changes and safe insets reach the same context. Keyboard state starts from
`Keyboard.isVisible()` when available and resynchronizes when the option is
enabled, with subscriptions removed on disable or unmount.

## Source, ownership and skills

Read the actual pinned `BottomTabView`, `BottomTabBar`, height contexts and
keyboard hook, the QNX adapter and the repository execution contract.
DRI: `hardening_prs`, acting as Metro / Expo developer tools engineer.
Independent review is assigned to the parent integrator before publication.

| Skill source                                                                               | Read and applied decision                                                                                                              |
| ------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------- |
| Repository `qnx-platform-validation` at base                                               | Use a real navigator integration and failing control; keep host results separate from device acceptance.                               |
| Margelo `api-design`, commit `1e9e17be6f41c838db472d6a4b943fc26355a3be`                    | Preserve the existing public options and report state through the navigator's existing callback contract.                              |
| Margelo `build-nitro-modules`, then `cpp`, same commit                                     | Read in the required order for the paired native review; this JS slice adds no Nitro spec, generated binding or native implementation. |
| Callstack `react-native-best-practices`, commit `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` | Demonstrate the stale scene inset through the actual caller before changing state; claim no frame-rate improvement.                    |

These are source reads, not skill installations. No external CodeRabbit review
is claimed by this receipt.

## Validation

Linux host, Node 26.13.1 and pnpm 12.18.0:

- Frozen offline install with lifecycle scripts skipped passed; all 524 packages
  were reused from the existing store and the lockfile was unchanged.
- `node tests/js/qnx-router-keyboard-space.test.mjs` passed for JavaScript and
  native-bar routing, the real navigator height context, an already-open keyboard,
  option changes, explicit hiding, rail layout and listener cleanup.
- `node tests/js/qnx-router-tab-options.test.mjs` passed.
- `node tests/js/expo-router-qnx.test.mjs` passed, including platform resolver
  isolation. Other platforms retain the upstream router.
- `pnpm typecheck`, changed-file Prettier and `git diff --check` passed.

The negative control ran the new test against the original adapter and failed
at the hidden-bar context assertion: `actual: 70`, `expected: 0`. The same test
passed with the adapter correction. Animation test doubles settle immediately;
these checks do not measure native animation timing or keyboard input latency.

No QNX cross-build, QNX VM, Pi 400, stock QSTI, physical keyboard, software
keyboard provider, Marathon shell or performance run was performed for this
follow-up. Hosted checks must run on the integrated PR head. This changes no
SDK payload, permissions, premium dependency or runtime distribution material.
Rollback reverts the adapter and test changes; no device state was modified.
