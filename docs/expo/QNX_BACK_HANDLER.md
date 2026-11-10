# QNX BackHandler experiment

`RNQNX_ENABLE_BACK_HANDLER=ON` connects the standard React Native
`BackHandler` API to **one explicitly attached Qt host window**. The option is
OFF by default. The playground attaches its own window after installing the
runtime binding. Embedders call `host.setBackHandlerWindow(&window)` after
`installTurboModuleBinding()`; pass `nullptr` to detach.

```js
import { BackHandler } from 'react-native';

// Last registered listener runs first; true consumes Back.
const subscription = BackHandler.addEventListener('hardwareBackPress', () => {
  if (!canGoBack()) return false;
  goBack();
  return true;
});
subscription.remove(); // Safe to repeat during cleanup.
```

```js
// Requests closure of this host's attached window, not process-wide exit.
// Throws if the host has detached or lost its window.
BackHandler.exitApp();
```

Accessing `BackHandler` when the native experiment is disabled throws an error
naming the build option instead of installing a no-op implementation. This
uses RN 0.88.1's own reverse-order subscription dispatcher and generated
`NativeDeviceEventManagerCxxSpec`; it is a React Native framework adapter, not
a new application-facing TurboModule or a substitute for Nitro v1.

## Input and lifetime behavior

- An unmodified, non-repeated Qt `Key_Back` release emits one
  `hardwareBackPress`; press and auto-repeat events do not navigate twice.
- Escape remains owned by the editor/modal/application. It is not remapped to
  system Back.
- Unhandled Back requests closure of the attached window. The same applies
  when the runtime emitter or its BackHandler listener has not been installed.
  Duplicate synchronous fallback calls during one dispatch close at most once.
- Events from other windows are untouched. Detaching/rebinding cancels queued
  events from the previous attachment. Destroyed windows cancel their pending
  events through guarded references.
- Native delivery uses the existing RuntimeScheduler invoker. No timer,
  synthetic timeout, background queue, or asynchronous acknowledgment protocol
  is introduced. JS handlers run synchronously as the RN contract requires;
  this does not implement a hung-JS watchdog or an OS Back acknowledgment API.
- Host teardown removes the Qt observer and invalidates accepted deliveries
  before Hermes shutdown. Later module release does not own a Qt observer.

## Validation

```sh
cmake --preset linux-host-renderer \
  -DRNQNX_ENABLE_BACK_HANDLER=ON \
  -DRNQNX_BUILD_BACK_HANDLER_TESTS=ON \
  -DRNQNX_BACK_HANDLER_SANITIZERS=ON
node scripts/validate-host-renderer.mjs
ctest --test-dir build/linux-host-renderer -R '^rnqnx_back_handler_' \
  --no-tests=error --output-on-failure -V
```

Repeat with `RNQNX_ENABLE_BACK_HANDLER=OFF`. The existing AppState CI matrix
runs both BackHandler configurations and publishes exact-run logs. Tests load
real production Metro bundles for the QNX and Marathon platform identities in
Hermes, drive synthetic Qt keys, and check listener order, consumption,
removal, fallback, unrelated windows, repeat/Escape exclusion, detached errors,
queued cancellation, window destruction, and post-invalidation destruction.
Sanitizers instrument this adapter and test, not prebuilt Qt/RN/Hermes.

## Remaining Router requirements

This is not complete Expo Router qualification. SDK 58.0.22's bundled module
manifest selects Expo Router `~58.0.29`, Expo Linking `~58.0.15`, Screens
`~4.30.1`, and Safe Area Context `~5.10.1`. The inspected Router 58.0.29
package contains its own navigation fork (`build/react-navigation`) and uses
`standard-navigation`; old React Navigation 7 assumptions do not establish
its contracts.

OS URL activation/initial URL, native safe-area providers, Screens/native-stack
registration, modal Back priority, route restoration and actual Router stack,
tabs and deep-link execution remain unimplemented or unqualified here. No
physical Back key, QNX device or Marathon shell result is claimed by synthetic
host tests. The open-core Router readiness gate remains pending.

The opt-in public RN entry also exposes the existing `Dimensions`, `PixelRatio`
and `useWindowDimensions` implementations for navigation layout consumers.
They use the existing Qt `DeviceInfo` metrics; exposing them does not qualify
window-resize/keyboard/safe-area event delivery. The public-entry probe now
checks actual initial metrics and renders through the upstream hook.

## Source and skill decisions

- RN 0.88.1: `BackHandler.android.js`, `HardwareBackPressEvent`,
  `RCTDeviceEventEmitter`, and generated `NativeDeviceEventManagerCxxSpec`
  establish the framework contract. QNX retains its own platform identity.
- Margelo `api-design`, `build-nitro-modules`, `cpp` at
  `1e9e17be6f41c838db472d6a4b943fc26355a3be`: retain the upstream caller API,
  expose cleanup, and destroy the Qt-owned observer before runtime disposal;
  no new native API/generator/ABI was invented.
- Callstack `react-navigation`, `react-native-best-practices`, and
  `github-actions` at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`: inspect the
  actual Router version, reuse existing runtime delivery, and preserve
  exact-head ON/OFF artifacts rather than claiming mobile/device coverage.
- Repository `qnx-native-api`, `qnx-platform-validation`, ADR-0004 and the
  Nitro v1 policy: framework-only adapter, explicit unavailable mode, no
  premium dependency, separate host/device evidence.

Rollback disables `RNQNX_ENABLE_BACK_HANDLER`; no data, credentials, OS
configuration, dependency pins, or renderer architecture changes are required.
