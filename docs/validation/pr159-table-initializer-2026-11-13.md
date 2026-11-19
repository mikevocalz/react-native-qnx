# PR 159 component table build repair

Base: `7ae99da` on `feat/expo-image-painted-view`, reviewed November 13, 2026.
The enabled renderer CI failed in `QtExtraComponents.cpp`: the video row
omitted the new `dispatchCommand` field. Clang treats the missing initializer
as an error under the repository's strict warnings. The row now explicitly
uses `nullptr`, matching the other features without view commands.

This changes no public API, renderer, feature default, dependency or window
contract. Video commands still use their existing bridge. The Expo Image
command handler remains registered in its own row.

## Validation

- Apple Clang strict syntax check of the production component table, with
  VIDEO, EXPO_IMAGE and NATIVE_UI enabled: passed. It uses real Qt Core
  headers and a test-only minimal ShadowView declaration, so it proves the
  aggregate/table compilation only. Full linked runtime checks belong to CI.
- `node tests/js/host-renderer-contract.test.mjs`: passed.
- `node tests/js/qt-video.test.mjs`: passed.
- `node scripts/validate-docs.mjs` and `git diff --check`: passed.
- Frozen dependency installation with pnpm 12.18.0: passed. Lock SHA-256:
  `4a4695a15a04a2ec9dcfca49b8fa1cf29c624feaf8281f91b3881309000439f2`.
- QNX cross-build, physical QNX and Marathon runs: not run for this repair.
  No target or release gate is promoted. Inspect the new head's hosted
  renderer result after pushing.

## Skills and ownership

Repository `qnx-native-api` and `qnx-platform-validation` were read. The
Margelo `api-design`, `build-nitro-modules`, `cpp` sources at
`1e9e17be6f41c838db472d6a4b943fc26355a3be` were read and their bytes verified
against `skills.lock.json`. Applied decision: preserve the existing API and
registration boundary; fix the initializer without weakening compiler flags.
Callstack `react-native-best-practices` at
`59e9d17446a248b21d86f078fcd0bd2d36db1ca4` was read; no performance claim
follows from this compile repair. The installed `no-ai-slop` skill was read
and applied to this receipt. No new skill installation is claimed.

DRI: current integration agent, component table path only. Independent
review: CodeRabbit CLI 0.7.6 reviewed the tracked repair and handoff changes,
with zero findings. Human integration review remains unassigned. Security, licensing and performance
behavior are unchanged; no premium source is involved. Rollback is the single
initializer change. Continue the image-loader gap list in today's handoff;
this repair alone does not finish PR 159.
