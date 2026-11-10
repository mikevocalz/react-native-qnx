# Experimental inbound Linking provider

`RNQNX_ENABLE_LINKING=ON` adds RN 0.88.1's generated
`NativeLinkingManagerCxxSpec` framework adapter. It is OFF by default. This is a
Router prerequisite, not an Expo Router or device qualification claim.

## Contract

The public `react-native` `Linking` export uses upstream RN's implementation,
including Promise results, URL events and removable JS subscriptions. With the
flag OFF, importing Linking remains safe for ordinary RN component imports;
calling any Linking operation throws an explicit unavailable-feature error.

On the Qt application owner thread, after `installTurboModuleBinding()` and
before bundle evaluation, configure the one application receiver:

```cpp
host.configureLinking({"myapp"}, std::string("myapp://room/42"));
```

The optional initial URL is an immutable launch snapshot. `getInitialURL()`
resolves it repeatedly, or resolves `null` when no launch URL was supplied.
Only the configured host observes incoming `QFileOpenEvent` URL activations.
Accepted activations emit the standard `url` event with `{url}`. A second host
cannot silently replace the receiver; destruction releases ownership.

Configuration permits 1–16 custom schemes, each at most 64 characters. Shared
web, file and other reserved schemes are rejected. URLs must be absolute,
valid, match the configured scheme, have no user-info credentials, and fit in
8192 encoded bytes. At most 32 event deliveries can be pending. Overflow and
unrecognized events are not consumed. The adapter does not log URL contents.
It does not authenticate route parameters; applications must still authorize
access to the destination.

Host event delivery first posts through `QtRuntimeTaskQueue`, then enters the RN
scheduler on the owner. It never executes application JS inline inside the Qt
event filter. Throwing JS listeners use the host scheduler error boundary.

Invalidation removes the event filter and destroys its QObject on the Qt owner
thread. Weak queued deliveries cannot touch a disposed runtime. Later module
release on a GC/worker thread is safe after explicit invalidation.

`openURL`, `canOpenURL` and `openSettings` reject with an explicit unavailable
error. No QNX platform URL/settings dispatcher has been integrated. Returning
`true`, guessing OS capabilities, or silently invoking desktop behavior would
misrepresent target support.

## Playground

With the feature enabled, the playground accepts repeatable `--url-scheme`
and optional `--initial-url` arguments. These are explicit host inputs, not
proof that the OS launches the application for a scheme. Do not put secrets in
launch arguments. Ordinary invalid configuration diagnostics omit the URL.

## Validation and remaining boundaries

Configure `RNQNX_BUILD_LINKING_TESTS=ON` with a Linux native host graph to build
`rnqnx_linking_runtime_test`. The existing runtime-appstate CI matrix runs
feature ON/OFF, qnx/marathon Metro bundles, and adapter/test ASan+UBSan. Instrumented
adapter evidence does not instrument prebuilt Qt, RN or Hermes dependencies.

Tests exercise the actual RN public import and Hermes runtime, initial URL
Promises, real rejected outbound Promises, incoming Qt events, subscription
removal, rejected URLs, bounded pending work, owner takeover refusal, immutable
configuration, queued invalidation and post-invalidation worker release.

These events are **synthetic Qt activation**, not OS registration or physical
QNX/Marathon evidence. App scheme registration, QNX launch/activation delivery,
universal links, outbound system dispatch and full Expo Router navigation
remain unqualified. Do not promote those acceptance gates from a host test.

Source audit: RN 0.88.1 `NativeLinkingManager` JS spec, generated
`FBReactNativeSpecJSI.h`, upstream `Linking.js`/`NativeEventEmitter`, and Qt 6.11.1
`QFileOpenEvent`, `QUrl`, QObject event filter/owner-thread APIs. This adapter
implements an existing RN framework contract under ADR-0004, not a new public
application TurboModule or a substitute custom router.
