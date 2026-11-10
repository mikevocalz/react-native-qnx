# Expo QNX implementation slices, 2026-11-10

This records executable changes on the PR #113 branch, not completed QNX
qualification. No acceptance gate is promoted by this record.

## Implemented here

- External consumer development: `rnqnx project start` serves the consumer's
  own installed Metro/config on loopback, reports bundle/source-map URLs,
  supervises process cleanup, and supports cache reset. It does not implement
  the native Fast Refresh connection or claim retained application state.
- Public React Native navigation prerequisites: expose the existing viewport
  and layout APIs through the public entry instead of forcing deep imports.
  Add a default-OFF native BackHandler adapter for a specifically attached Qt
  window, using the generated RN DeviceEventManager contract and existing
  JavaScript subscription semantics. Window rebinding and teardown invalidate
  pending deliveries; Linux ON/OFF runtime tests exercise the contract. This
  does not create a Router stack or an OS deep-link provider.
- Physical-input prerequisite: the experimental input-lifecycle path uses Qt
  global coordinates and observed pressure, and suppresses duplicate
  Qt-synthesized mouse delivery. Native and JavaScript payload assertions are
  registered; physical HID and IME remain separate acceptance work.
- Expo missing-service behavior: QNX public Expo native-module resolution
  retains the upstream registry/installer and returns explicit unsupported
  errors rather than pretending a Camera, Sensor or Notifications backend
  exists. Optional lookup remains optional. No permission is granted by this
  diagnostic and no mock native module is installed.
- Binary deployment: verify staged bytes by round trip before activation,
  serialize operations per app, retain a previous executable, and support
  explicit rollback without a local build. Host tests inject transfer,
  backup-copy and activation failures. No target was contacted by those tests.

## Remaining work by gate

| Core gate       | Still required; not implied by these changes                                                                                                                                                                  |
| --------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| External app    | Licensed QNX clean-host compile and real external-app launch with binary, bundle and asset identities.                                                                                                        |
| Autolinking     | Execute generated module discovery, invocation and disposal on the named QNX target; host resolver/CMake tests alone are insufficient.                                                                        |
| Router          | Real SDK 58 Router stack/tab/modal/link integration, safe-area/screen native providers and target restoration. Public viewport exports and Back handling are prerequisites, not Router parity.                |
| Developer loop  | Integrate and qualify the native development runtime, Metro reconnection and Fast Refresh state retention. Serving a development bundle is not native execution.                                              |
| Input/gestures  | Physical mouse/keyboard/touch and IME/accessibility providers; actual RNGH recognition/arbitration. Synthetic Qt tests are not physical device evidence.                                                      |
| Worklets        | Real Worklets runtime/bootstrap/scheduling and bytecode delivery; Reanimated Fabric/platform callbacks and RNGH platform recognition. These are missing implementation, not merely missing hardware receipts. |
| Native services | Named per-module native capability, denial and lifecycle behavior. Camera, Sensors and Notifications backends are not created by explicit unavailable errors.                                                 |
| Distribution    | Versioned JS/HBC/assets, Qt library/plugin closure and license/source-replacement materials, clean-host reproduction and complete-bundle rollback. The new rollback handles only the executable.              |
| Performance     | Twenty supervised cold boots and target startup/presented-frame/RSS capture for each workload. A JavaScript timer or queued render is not physical presentation evidence.                                     |

The Worklets/Reanimated/RNGH ports must follow the repository's native API
policy and audited upstream types. They cannot be supplied by renaming Android
or Apple implementations, substituting NativeAnimated, or introducing a Nitro
facade with no genuine third-party runtime. The native bootstrap architecture
exception requires a reviewed ADR and a separately compiled, default-OFF slice.

The optional Nitro Forge consumer remains separately licensed and is not an
open-core build or release prerequisite. No merge, target deployment, migration
or release publication is performed by this work.
