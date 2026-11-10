# Native Metro full reload experiment

This default-OFF host Debug slice downloads a development bundle asynchronously
from explicit `http://127.0.0.1:<port>/*.bundle?platform=qnx&dev=true`, evaluates it
in the existing Hermes/Fabric/QtQuick host, and supports F5 full reload. **This is
not Fast Refresh**: reload discards the old Hermes runtime and React state. It
adds no HMR websocket, native DevSettings module or automatic source-edit update.

Configure a compiler-enabled native Linux/Darwin Debug host with
`RNQNX_ENABLE_METRO_RELOAD=ON`; cross-build, QNX and release modes are rejected.
Start the existing `rnqnx project start` loopback Metro server, then pass its URL
as `--dev-bundle-url` to the playground/consumer host. `--bundle`, `--screenshot`
and `--trace-json` cannot be combined with this development mode. F5 downloads
a fresh bundle; compile errors/timeouts report status, with up to three bounded
idempotent GET attempts. After correcting the source/server, F5 retries again.

## Ownership and limits

The native session and every method run on the owning Qt thread. Network work is
asynchronous. Replies have a 15-second deadline and a 32-MiB byte limit (both
validated and bounded). Redirects, remote hosts, credentialed URLs and invalid
or duplicate required query fields are rejected. It never changes firewall,
network permissions or existing server bindings. The session disables inherited
proxy routing for its own network manager, without changing global proxy settings. Use only your trusted Metro
source: successful downloads execute code.

The mounting adapter, window and input dispatcher outlive the session. Reload
requested from a JS native callback is deferred until the runtime task is idle.
Pending HTTP replies are aborted and generation-guarded. Accepted completion
queue work drains before old runtime destruction; late old-queue work rejects.
Stop/destruction are native-owner lifecycle operations outside JS execution.
Listeners get one immediate snapshot and subsequent queued statuses, with
idempotent cleanup handles. Listener callbacks must not throw. All surfaces use the existing renderer, not a second
view tree. The prepare callback reattaches host services for each generation.

## Source / policy receipts

DRI: devloop worker; independent reviewer: pending parent assignment.
Adapted only the bounded lifecycle/fetch portion of PR #110 head
`d5d81908a83d4f2fbc022c5915d6f6ae23193192`; did not merge its dev-runtime stack.
Current RN 0.88 headers, `QtReactHost`, `QtRuntimeTaskQueue`, existing native test
fixtures and Qt6.11.1 network APIs were inspected. No dependency version or ABI
upgrade and no app-facing native binding were introduced. This is internal host
infrastructure, not a new legacy module or fabricated Nitro v1 binding.

Margelo source `1e9e17be6f41c838db472d6a4b943fc26355a3be`, ordered native pipeline:
`api-design` informed explicit lifecycle and listener cleanup;
`build-nitro-modules` kept native ownership separate from unverified generated
binding shapes; `cpp` informed RAII and one owner thread. Repository native API,
platform validation and Nitro v1 policy apply. Core does not require Forge.

## Validation

`rnqnx_metro_reload_test` covers real Hermes evaluation over a local HTTP fixture,
full reload state reset, JS-callback reload deferral, queue teardown, stale fetch
cancellation, HTTP error recovery, timeout/byte limits and URL validation.
Passing a real Metro URL as its only argument additionally loads the public
React Native `metro-reload-probe.js`, mounts a Fabric View, reloads the real
bundle, verifies fresh state and checks mount failures. Synthetic lifecycle
results alone do not prove Metro or React integration.

Host Clang/Qt/Hermes results, exact commands and logs are reported separately.
Physical QNX/Marathon, state-preserving Fast Refresh, HMR/reconnect after an
already-ready app and inspector support remain unproven/not implemented here.
Rollback: disable `RNQNX_ENABLE_METRO_RELOAD`; normal file-bundle path is unchanged.

### Host result receipt (2026-11-10)

- Clang 19, Qt 6.11.1, RN 0.88.1, pinned Hermes `9a115f15785851f4ff2747ebb090ddd066fc8ad2`.
- Strict compilation: own host, session, native tests and playground runner PASS.
- Native lifecycle fixture PASS: `RNQNX_METRO_RELOAD_HOST_OK`.
- Real HTTP Metro 0.87.1 dev bundle PASS: `RNQNX_METRO_REAL_RENDER_RELOAD_OK`, exit 0, two Fabric mounts and reset state.
- Session/tests use ASan+UBSan; host/common runtime uses ASan. External Qt/RN/Hermes are not claimed fully instrumented. Whole-host UBSan was first attempted and exposed shared-pointer template interposition into Hermes built without RTTI; that incompatible instrumentation is not presented as product failure or a sanitizer pass. No sanitizer suppression was added.
- Isolated CMake configure OFF and native Debug ON PASS; Release and QNX/cross configurations rejected. This configure fixture is not a QNX compile.
- The first real dev render found the shared eager-throw BackHandler defect, resolved by the independently tested navigation import-safety fix `fb06ffb` (equivalent main `b9a5d50`), not by disabling React's development container.
- Fontconfig cache write warnings are emitted in this read-only host environment. No physical display or target-device result is claimed.

Reproduce the real server/native test with:

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  node tests/native/metro-reload/run-metro.cjs /path/to/rnqnx_metro_reload_test
```

The script owns Metro and its shutdown and uses a dynamically assigned loopback
port. A separate manual `rnqnx_metro_reload_test` run exercises negative network
and native lifetime paths. Native executable/library build and runtime paths
must come from the same aligned host graph.
