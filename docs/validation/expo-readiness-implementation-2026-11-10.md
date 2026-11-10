# Expo QNX implementation slices, 2026-11-10

This records executable changes on the PR #113 branch, not completed QNX
qualification. No acceptance gate is promoted by this record.

## Implemented here

- External consumer development: `rnqnx project start` serves the consumer's
  own installed Metro/config on loopback, reports bundle/source-map URLs,
  supervises process cleanup, and supports cache reset. A default-OFF native
  Debug session now fetches bounded loopback bundles and performs real full
  Hermes/Fabric reload with cancellation and fresh state. Native lifecycle and
  actual Metro development render/reload tests pass. It does not implement
  Fast Refresh/HMR or claim retained application state.
- Public React Native navigation prerequisites: expose the existing viewport
  and layout APIs through the public entry instead of forcing deep imports.
  Add a default-OFF native BackHandler adapter for a specifically attached Qt
  window, using the generated RN DeviceEventManager contract and existing
  JavaScript subscription semantics. Window rebinding and teardown invalidate
  pending deliveries; Linux ON/OFF runtime tests exercise the contract. This
  does not create a Router stack or an OS deep-link provider. Default-OFF
  Linking now supplies real initial-URL Promises and scoped Qt URL events with
  explicit unsupported outbound dispatch. Navigation ON/OFF native tests and
  default-OFF full app rendering pass for both public platform bundles.
- Physical-input prerequisite: the experimental input-lifecycle path uses Qt
  global coordinates and observed pressure, and suppresses duplicate
  Qt-synthesized mouse delivery. Native and JavaScript payload assertions are
  registered; physical HID and IME remain separate acceptance work.
- Expo missing-service behavior: QNX public Expo native-module resolution
  retains the upstream registry/installer and returns explicit unsupported
  errors rather than pretending a Camera, Sensor or Notifications backend
  exists. Optional lookup remains optional. No permission is granted by this
  diagnostic and no mock native module is installed.
- Real Expo application metadata: default-OFF `ExpoApplication` uses Qt
  application identity, display name and version, with null for unknown metadata.
  Native Qt/Hermes ON and OFF tests and an official SDK public-source fixture
  pass. This is one real module, not a Camera/Sensors/Notifications backend.
- Frame observations: default-OFF scoped Qt frame-swapped tracing records
  monotonic native observations with bounded output. Actual Qt host and sanitizer
  tests pass; frame-swapped timing is not physical panel presentation evidence.
- Binary deployment: verify staged bytes by round trip before activation,
  serialize operations per app, retain a previous executable, and support
  explicit rollback without a local build. Host tests inject transfer,
  backup-copy and activation failures. No target was contacted by those tests.
- Offline distribution: a versioned payload manifest verifies executable,
  JS/HBC, declared assets, ELF dependency closure, modes and license-material
  inventory. Host tests cover tampering, dependency gaps and safe publication.
  It neither deploys nor proves target loader configuration, runtime discovery,
  ABI/bytecode compatibility, redistribution rights or authenticity.

- Real Worklets host integration: upstream 0.13.1 runs on a default-OFF Linux
  Qt/Hermes host, with owner scheduling, actual QQuickWindow frame delivery,
  UI-to-RN round trips, error reporting, reload and cancellation tests. Four
  actual native tests pass. QNX lean-Hermes bytecode/ABI integration remains
  explicitly rejected; this is not Reanimated or RNGH parity.
- Native Expo autolinking: generated provider registration now reaches real
  QtReactHost startup independently of ExpoApplication. ON/OFF native tests
  prove invocation and per-runtime disposal. This requires an explicitly
  ported QNX provider entrypoint, not arbitrary Android/iOS module compatibility.

## Remaining work by gate

| Core gate       | Still required; not implied by these changes                                                                                                                                                                                                                           |
| --------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| External app    | Licensed QNX clean-host compile and real external-app launch with binary, bundle and asset identities.                                                                                                                                                                 |
| Autolinking     | Execute generated module discovery, invocation and disposal on the named QNX target; host resolver/CMake tests alone are insufficient.                                                                                                                                 |
| Router          | Real SDK 58 Router stack/tab/modal/link integration, safe-area/screen native providers and target restoration. Public viewport exports and Back handling are prerequisites, not Router parity.                                                                         |
| Developer loop  | QNX development-runtime/bytecode integration, HMR reconnection and Fast Refresh state retention. Host full reload executes real development bundles but resets state; it is not target Fast Refresh.                                                                   |
| Input/gestures  | Physical mouse/keyboard/touch and IME/accessibility providers; actual RNGH recognition/arbitration. Synthetic Qt tests are not physical device evidence.                                                                                                               |
| Worklets        | QNX lean-Hermes bytecode/unpacker and SDP ABI integration; Reanimated Fabric/platform callbacks and RNGH platform recognition. Linux Worklets execution is proven, not target integration or full animation parity.                                                    |
| Native services | Named per-module native capability, denial and lifecycle behavior. Camera, Sensors and Notifications backends are not created by explicit unavailable errors.                                                                                                          |
| Distribution    | Target loader and resource-discovery qualification, ABI/bytecode compatibility, license/source-replacement compliance, clean-host reproduction and complete-bundle rollback. Offline manifest integrity is not launch evidence; deployed rollback remains binary-only. |
| Performance     | Twenty supervised cold boots and target startup/presented-frame/RSS capture for each workload. A JavaScript timer or queued render is not physical presentation evidence.                                                                                              |

The Worklets/Reanimated/RNGH ports must follow the repository's native API
policy and audited upstream types. They cannot be supplied by renaming Android
or Apple implementations, substituting NativeAnimated, or introducing a Nitro
facade with no genuine third-party runtime. The native bootstrap architecture
exception requires a reviewed ADR and a separately compiled, default-OFF slice.

The optional Nitro Forge consumer remains separately licensed and is not an
open-core build or release prerequisite. No merge, target deployment, migration
or release publication is performed by this work.
