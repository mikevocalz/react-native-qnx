# Experimental host WebSocket transport

Status: **draft, host-verified only; not accepted for QNX**.

`QnxWebSocketModule` can opt into a real Qt WebSockets backend. The default is
`RNQNX_ENABLE_WEBSOCKETS=OFF`: no WebSockets or Network dependency is added, and
socket operations throw an actionable unsupported-feature error instead of
silently succeeding. Listener registration remains compatible with RN's
`NativeEventEmitter`.

## Scope

The optional backend implements `connect`, `send`, `sendBinary`, `ping`, and
`close`, plus RN's `websocketOpen`, `websocketMessage`, `websocketClosed`, and
`websocketFailed` event contracts. Binary messages use base64 at the native
boundary. Subprotocols and ordinary request headers are supported; malformed
headers and reserved handshake headers are rejected. Blob integration is not
implemented.

All socket operations run on the Qt application thread. JSI arguments are copied
before commands are queued; events return through `CallInvoker` without retaining
JSI runtime references. Module destruction invalidates queued JS events and
native commands. Sockets have a 30-second opening deadline and a 5-second closing
deadline. Incoming and individual outgoing messages are limited to 16 MiB.
Connection failures emit one terminal failure event (RN itself turns this into
both JS error and close); ordinary close handshakes emit one closed event. A
repeated close after termination is harmless. Duplicate live native socket IDs
terminate the original with a failure instead of silently replacing it.

TLS uses Qt's default peer verification. Certificate errors are never ignored.
The host test rejects an ephemeral, self-signed local TLS endpoint; trusted remote
`wss://` interoperability and target CA-store provisioning remain integration work.

## Reproduce host checks

Use Qt **6.11.1**, including `Core`, `Network`, and `WebSockets`, the repository's
Node/pnpm versions, CMake 3.31.9 or later, a C++20 compiler, and OpenSSL's CLI and
Qt TLS backend. Install the exact pnpm lockfile before enabling JSI tests.

```sh
cmake -S . -B build/websocket -G Ninja \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.11.1/gcc_64 \
  -DCMAKE_BUILD_TYPE=Release \
  -DRNQNX_ENABLE_WEBSOCKETS=ON \
  -DRNQNX_FETCH_REACT_NATIVE_DEPS=OFF \
  -DRNQNX_BUILD_WEBSOCKET_JSI_TESTS=ON
cmake --build build/websocket --parallel 2 --target \
  qt_websocket_transport_host_test \
  qnx_websocket_module_enabled_host_test \
  qnx_websocket_module_disabled_host_test
ctest --test-dir build/websocket -R websocket --output-on-failure
```

`RNQNX_BUILD_WEBSOCKET_JSI_TESTS` is separately opt-in because it builds the
repository-pinned Hermes revision. It tests the actual RN TurboModule dispatcher,
JS event shapes, disabled-feature error, invalid arguments, and queued-callback
invalidation, including creation and destruction on a separate JS thread. It does not replace the complete Fabric/renderer test suite.

The standalone transport test uses real loopback WebSocket servers for text,
binary, headers, subprotocols, concurrent sockets, ping, close, connection refusal,
opening/closing timeouts, wire-level ping, cancellation, duplicate IDs, malformed
header tokens, message-size limits, destruction with work pending, and untrusted
TLS rejection. ASan and UBSan instrument the transport and
test/module translation units when `RNQNX_ENABLE_HOST_SANITIZERS=ON`; Qt and Hermes
are not themselves sanitizer-instrumented. Leak detection is disabled for
process-lifetime Qt/Hermes singletons, as in the other host tests.

## QNX acceptance boundary

Enabling this feature in a QNX CMake build intentionally fails configuration.
This is a host experiment and does not modify the accepted QNX module subset.
Before a later PR may enable QNX, it must satisfy
[Qt module build discipline](build-qt-modules.md):

- Confirm the licensed SDP and the same Qt 6.11.1 source tag on the target.
- Review Network and Tier 2 WebSockets licensing/justification against ADR-0011.
- Record before/after target binary-size and cross-build-time deltas, including
  transitive TLS and deployment dependencies. The current values are **TBD**.
- Cross-compile and validate deployment and CA-store/TLS configuration.
- Run device tests for networking, disconnect/reconnect, shutdown, and lifecycle.

Host loopback success is not evidence for any of those target gates. Removing
the CMake guard without the recorded evidence would bypass the acceptance policy.
