# Public Router starter and fresh external consumer, 2026-11-22

This is host-only source-distribution evidence. No package or runner was
published, no native app was launched and no release gate is promoted.

## Source and behavior

The generated Router starter now uses public `ExpoRoot`; registration is kept
in `index.js` and the component/context live in separate modules. The adapter's
lazy `QnxRouterProvider` preserves the exact installed SDK 58.0.31 Head provider
through `expo-router/head` and its layout-refresh compatibility hook. The one
private compatibility import stays inside the platform package. Normal Node
imports of unrelated adapter utilities do not initialize Router app globals.
The small example uses the same root split and retains its per-root Zustand
provider and configured component name.

`qnx-router-public-entry.test.mjs` runs an isolated fixture that checks lazy
loading, unchanged Head/context forwarding and the actual SDK refresh classifier
for layout exports. The actual archive/create regression verifies generated
public imports, separate root files, archive contents and exact native peer pins.
The independent window/software reviewer inspected the installed upstream
qualified entry and found no behavior blocker in this split.

## Fresh consumer

Created outside the checkout with the real CLI:

```sh
node packages/react-native-qnx-cli/src/bin.mjs project create \
  --project <external>/expo/fresh-consumer \
  --app-id org.rnqnx.freshconsumer --name 'Fresh Router'
```

The first lock resolution exposed auto-installed peers newer than the reviewed
platform: Gesture Handler 3.5.0, Reanimated 4.8.0 and Worklets 0.14.0. That attempt
was preserved and stopped. The generated consumer contract now pins the existing
source versions, 3.4.0, 4.7.1 and 0.13.1. Root workspace versions are unchanged.
A new consumer was generated, its own lockfile resolved, and its real frozen
installation completed with pnpm 12.21.0 and Node 26.13.0. The final public-Head
archive was minted through the generator and frozen-installed after refreshing
its local archive integrity. No installed platform source was hand-patched.

The installed platform resolves under the consumer's own `node_modules`, not
back into the source checkout. All three archive hashes were read back against
`platform-packages.json`. Packed runtime helper/index bytes equal the reviewed
source, and generated registration/root bytes equal the template after name
substitution. [Hashes and comparisons](source-and-artifact-hashes.json) bind the
actual archives, consumer lock/config/source and reviewed runtime source files.
[Initial frozen install](initial-frozen-install.log) and
[final archive frozen install](frozen-install.log) retain the installation results.
Source-code hashes identify the tested working changes; the commit containing
these files supplies their final repository identity.

## Actual bundling and Expo-owned reload protocol

The real `project bundle --project <external>/expo/fresh-consumer --out
build/qnx/final.bundle.js` command succeeds: **1,444,324 bytes**, SHA-256
`8daafb911dfcf9e04c29d1356436b6f34dcb5e26db3d6c78d6472642c50db4eb`.
The CLI resolves Metro 0.87.1 inside the consumer. No workspace source imports
are needed by its application or Metro config.

```sh
node scripts/probe-expo-qnx-dev-server.mjs \
  --project <external>/expo/fresh-consumer \
  --source 'app/(tabs)/index.js' --text 'The first tab.' \
  --out <NEW external evidence directory>
```

The [actual Expo CLI log](expo-start.log) and [result](dev-result.json) show QNX
development bundling, source watcher invalidation, original-source restoration
and identical initial/restored bundle hashes. A synthetic host WebSocket
observes the actual SDK v2 `reload` command rebroadcast by Expo's `/message`
server. It does not claim a native app received or executed that command.

Earlier receipts recorded HTTP 200 from `/reload`. That status alone did not
prove reload: SDK58 can answer it through fallback routing. This continuation
replaces that check with an observed v2 command, and adds a wrong-version
negative control that the actual SDK rejects. The lifecycle regression confirms
failed spawn/setup, rejected reload protocol and cancellation emit no successful
record, restore source and stop owned servers/worker groups. Successful evidence
is emitted only after bounded supervised cleanup and a closed-port check.

The final consumer's public Head export avoids the package-export fallback
warning observed during the earlier private-Head attempt. The remaining CLI
warning says a development-client scheme cannot be auto-discovered without
`expo-dev-client`; OS launch/deep-link behavior is still unverified.

## Scope, review and rollback

Repository Expo-delivery/platform-validation skills and pinned Expo Router/UI/
module sources read in the earlier software receipt apply here. The decision
changed by the inspected SDK entry is preserving Head/layout-refresh behavior
while moving consumer code to public exports. No skill installation, upstream
QNX endorsement, native ABI upgrade or second renderer is claimed. Existing
window sizes/orientation, native flags, recovery boot and Qt build recipes are
unchanged. No premium package is included.

The platform adapter is pinned to Router 58.0.31; an SDK change requires another
entry audit. Actual native Router rendering, native Fast Refresh delivery,
prebuilt runner distribution and named-device acceptance remain open. Revert
this slice to restore the previous starter entry; preserve the recorded peer
pins unless the replacement graph has been reviewed and tested.
