# Expo Router example software slice, 2026-11-22

Base: `20a631fd0001fb02e75b9b66014371ea5c9b3da5` on the existing distribution
continuation. DRI: Expo software subagent; independent review remains required
before the combined PR is ready. This receipt does not change release gates.

## Implementation

`apps/expo-router-example` previously held only `.gitkeep`. It now contains a
source-checkout application with two file routes, a root Tabs layout and public
`ExpoRoot`, `Tabs` and `Link` imports. Application components import React Native
from its public entry. State belongs to a vanilla Zustand store retained in a
root ref; selectors subscribe to note/counter data and actions. Separate roots
do not share drafts. Route remounts retain the current root's state; closing the
root releases that state. No state is serialized.

The generated QNX Router starter now uses `useWindowDimensions` instead of
introducing React `useState` for width. The adapter guide points to the existing
`project create` scaffolding rather than describing it as future work.

No native API, runtime, CMake flag, app-window dimension, orientation policy,
boot configuration or physical-target state changed. The example's new app
configuration uses its own identity and `orientation: default`.

## Host validation

Executed on macOS with Node 26.13.0 against the pre-existing installed SDK 58
graph before its relocation to external storage:

- `node tests/js/qnx-router-example.test.mjs`: passed actual route component
  handlers, edit/reset, remount retention, root isolation/teardown and narrow/wide
  tab layout. Native leaves and Router are host stand-ins. This does not prove
  actual Router navigation or native rendering.
- `node tests/js/expo-router-qnx.test.mjs`: passed existing resolver/adapter tests.
- `node tests/js/qnx-project-create.test.mjs`: passed real local archive,
  identity, no-overwrite and source-project creation checks; no native runtime.
- `node scripts/validate-docs.mjs`: passed 24 components and 11 gates.

After external-storage relocation, the integrator regenerated the lock and ran
the frozen pnpm 12.21.0 install. The new app uses existing pinned versions. Concurrent native-window integration
changes require a final combined bundle and CI run separately from this slice.
Lockfile SHA-256 at this probe: `33349bcbe82a58018b5c18810af1931b73fc69b2352c1be384a20e8f4c3b95b5`.

- Initial isolated-example production Metro QNX graph: 971 modules, 1,383,475-byte bundle,
  SHA-256 `0164d48e2926a08cdc5097a986314869f22ce9a273f1550c617571dedcfce0c6`.
  Command, run in the example directory: `node ../../node_modules/metro/src/cli.js
build index.js --config <example>/metro.config.js --platform qnx --dev false
--max-workers 2 --out <external>/router-example.bundle.js`. This is Metro
  bundling, not a native render or CLI project-bundle result.
- The original Expo-owned Metro server returned HTTP 500 for `platform=qnx`:
  `getSupportPackageForPlatform` had no QNX host-package identity. The original
  behavior is preserved in the [HTTP response](../evidence/expo-router-software-2026-11-22/unpatched-http-response.json).
- A version-pinned `expo-modules-autolinking@58.1.0` patch supplies only QNX host
  identity, `@rn-qnx/react-native-qnx`. Source TypeScript, built JavaScript and
  declarations agree. The QNX native linking implementation stays rejected.
  `node tests/js/qnx-expo-host-package.test.mjs` fails on the original unknown-QNX
  error and passes after the frozen patched install, checking QNX identity,
  existing host platforms, unknown-platform refusal and native-link refusal.
- `node scripts/probe-expo-qnx-dev-server.mjs --out <NEW external directory>`
  passes against the actual Expo CLI: HTTP 200 QNX development bundle, real
  source edit observed by its watcher, exact original source restored, restored
  bundle SHA equal to the initial bundle, host `/reload` response 200. This
  probe runs offline and disables telemetry. It never launches a native app.
- `node tests/js/qnx-expo-probe-lifecycle.test.mjs` verifies missing-interpreter
  spawn failure, a real injected post-spawn metadata write failure, and SIGINT
  cancellation while a route is temporarily edited. Every failure restores the
  source, emits no successful result, and closes its server. The cancellation
  test observes the supervisor's Expo session group and rejects any remaining
  live Expo/worker group member after shutdown.
- A SIGINT cancellation control while the marker was in the route source
  restores the exact original source and closes the owned server. Cancellation
  exits nonzero. The script uses atomic source replacement, refuses concurrent
  source changes and performs server shutdown even if restoration refuses.
  The Python guard supervises the CLI in its own session, reserves the leader
  PID during TERM/grace/KILL cleanup, and leaves unrelated groups alone. The
  JavaScript probe has an explicit child error listener, bounded shutdown waits,
  a closed-port assertion and a strict initial/restored bundle hash assertion.
  It writes successful evidence only after cleanup, with `serverStopped: true`.
  Setup files written after spawning are inside the cleanup-protected block.

[Host result](../evidence/expo-router-software-2026-11-22/host-result.json),
[Expo CLI log](../evidence/expo-router-software-2026-11-22/expo-start.log) and
[cancellation result](../evidence/expo-router-software-2026-11-22/cancel-result.json)
retain the observations. The CLI warns that the custom development-client
scheme cannot be auto-discovered without `expo-dev-client`; OS launch/deep-link
acceptance is not inferred. A host reload response does not prove a native
client received it. Native Fast Refresh delivery remains untested.

No QNX cross-build, VM, physical board, OS deep-link or native runtime interaction
result is claimed here.

## Skills and source inspection

Repository skills `qnx-expo-phase-delivery` and `qnx-platform-validation` at the
base above were read and applied: preserve public consumer imports, existing
QNX renderer and separate host/device acceptance. Pinned Expo skills were read
from `expo/skills` at `a9e5bd3fa00b4e61f632e43f85da73aee1942582`, paths
`plugins/expo/skills/expo-router/SKILL.md` and
`plugins/expo/skills/expo-native-ui/SKILL.md`. No skill installation is claimed.
Applied decisions: routes only under `app/`, shared components/state outside it,
public Link navigation and measured window dimensions. Mobile-only native Stack,
NativeTabs, SwiftUI/Compose controls, Expo Go and platform colors are not QNX
capability evidence. The existing QNX adapter explicitly documents Stack and OS
deep-link limits, so this example uses supported Tabs and literal fallback colors.
Installed `expo-router` 58.0.31 exports and ExpoRoot props were inspected before
using its public API. The pinned `expo-module` skill at the same Expo revision,
`plugins/expo/skills/expo-module/SKILL.md`, was read before auditing the host
identity adapter: discovering a host package does not register native modules.
No native bindings or Nitro API changes were made.

## Remaining acceptance and rollback

This source example does not publish workspace packages or runners. The Expo
release gate still requires published verified runners, full Expo CLI lifecycle
proof, native-module checks and native execution, external-consumer acceptance,
Fast Refresh and physical release-board execution. Existing tab/link scope does
not claim native stack, modal or OS deep-link support.

Rollback removes the example and test and restores the starter/guide changes.
There is no user data migration or deployed native change.
