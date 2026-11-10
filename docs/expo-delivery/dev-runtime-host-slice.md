# Host development runtime slice (default OFF)

Status: implemented, with original-author Linux native test evidence at
`083c4bd`. The combined PR #108/#110 candidate passed 47/47 enabled and 25/25
disabled local native tests, including the new development-metrics fixture in
the enabled run. Published-head CI is pending. The runtime remains host-only and defaults
OFF; QNX and other cross builds are rejected. A state-preserving Fast Refresh
edit has not been demonstrated.

## Protocol and source references

- React Native 0.88.1 `Libraries/Core/setUpReactRefresh.js`: Refresh hook injected before the renderer; requires `DevSettings.reload`.
- React Native 0.88.1 `Libraries/Utilities/DevSettings.js`: `reloadWithReason` and `onFastRefresh` delegate to native `DevSettings`.
- metro-runtime 0.87.1 `src/modules/HMRClient.js`: `update` injects `added`/`modified` through `globalEvalWithSourceUrl(code, sourceURL)`.
- Metro 0.87.1 `HmrServer.js`: `register-entrypoints` with the exact bundle URL; replies `bundle-registered`, `update-start`, `update`, `update-done`, `error` (`GraphNotFoundError`, `RevisionNotFoundError`).
- Hermes 9a115f15785851f4ff2747ebb090ddd066fc8ad2: the lean backend cannot compile source (`HBCStub.cpp`), so development needs `hermesvm`.

## Lifecycle

`DevSession` (owner Qt thread) fetches the `dev=true&hot=true&platform=qnx` bundle asynchronously with a deadline, byte limit and bounded retries. Every start/reload/stop increments the runtime generation; stale replies and reports are dropped. Reload waits for the host's JS idle boundary, calls `__RNQNX_DEV_DISPOSE__`, destroys the host (closing its task queue), and creates a new one on the same window and mounting layer. Metro reconnects use a separate connection generation with capped backoff. Graph loss requests one full reload.

Run: `rnqnx_playground_host --dev-bundle-url <url>` (always fullscreen; F5/Ctrl+R reload resets state).

## Native dimensions across startup and reload

In the combined adaptive host, the playground supplies
`DevSession::Configuration.window` and `.client`. Each new runtime binds the
existing `QtReactHost::observeWindowMetrics(surfaceId, window, client)` before
host preparation and bundle evaluation. Module-level DeviceInfo/Dimensions
reads therefore receive the actual React client bounds, window DPR and window
screen. The observer then updates the same Fabric surface and standard
Dimensions events. A separate development resize timer is no longer used.

A custom native session can omit both configuration fields and keep its
explicit `resize(width, height)` behavior. Supplying only one field fails
validation. A session with native geometry rejects manual resizing; change its
actual window/client instead. The pointers are guarded and the existing
observer verifies window/client ownership on the Qt thread.

If initial client bounds are zero, the successfully fetched bundle remains
within the configured byte limit and waits for a positive native size. The
session does not evaluate application code with guessed startup dimensions.
Size notifications coalesce into a generation-checked startup callback.
Stop/reload and window/client destruction cancel pending fetch/source work;
an older generation cannot evaluate when geometry becomes usable later.
Synchronous cancellation from `prepareHost` also closes admission before any
remaining runtime setup. After startup, only the native observer resizes the
surface.

## Validation and remaining work

The [original PR #110 report](https://github.com/mikevocalz/react-native-qnx/pull/110)
records 23/23 Linux CTests with `RNQNX_ENABLE_DEV_RUNTIME`, Modal focus, frame
trace and WebSockets ON, and 17/17 with those options OFF, using clang 14,
Qt 6.11.1 and ASan/UBSan. The report excludes two existing targets that failed
to compile with that clang/libstdc++ combination and describes an ASan
kernel-setting requirement. This evidence belongs to `083c4bd`; it does not
establish a full native pass on the current integration follow-up.

The earlier PR #110 integration's host-renderer inventory, controlled-Switch
JavaScript and type checks passed. Those source checks do not exercise
`DevSession` in a native process.
The refreshed full host-renderer workflow includes the development runtime and
WebSockets in its ON configuration and runs for stacked PRs; its new candidate
result remains pending. See the [validation handoff](../validation/pr110-merge-readiness-2026-11-10.md)
for the separate evidence sources and exact commands.

Run an actual host edit/reload session to verify state-preserving Fast Refresh,
error recovery, reconnect and native teardown before documenting those as
validated user workflows. The owner's QNX smoke report does not exercise this
host-only runtime. No QNX development-runtime support or debugger attachment
result is claimed.

The [PR #108 integration receipt](../validation/pr108-integration-2026-11-10.md)
tracks the added real Qt/Hermes/DeviceInfo fixture, including zero-size startup,
blocked-generation cancellation, initial values, native resize and reload.
Its bootstrap/AppRegistry stand-ins isolate native lifecycle/metrics and do not
prove the real Metro editing workflow. The finalized fixture passed in the
47/47 ON run, and its unobserved-metrics control exited 1 at the intended initial
client-dimensions assertion. The receipt preserves the initial LeakSanitizer
ptrace-host failures and process-local leak-disabled rerun; no leak-detection
proof is claimed.
