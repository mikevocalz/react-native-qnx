# Host development runtime slice (default OFF)

Status: implemented, with original-author Linux native test evidence at
`083c4bd`. The updated integration candidate has passed focused source checks;
fresh full native CI is pending. The runtime remains host-only and defaults
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

## Validation and remaining work

The [original PR #110 report](https://github.com/mikevocalz/react-native-qnx/pull/110)
records 23/23 Linux CTests with `RNQNX_ENABLE_DEV_RUNTIME`, Modal focus, frame
trace and WebSockets ON, and 17/17 with those options OFF, using clang 14,
Qt 6.11.1 and ASan/UBSan. The report excludes two existing targets that failed
to compile with that clang/libstdc++ combination and describes an ASan
kernel-setting requirement. This evidence belongs to `083c4bd`; it does not
establish a full native pass on the current integration follow-up.

The integrator's current host-renderer inventory, controlled-Switch JavaScript
and type checks passed. They do not exercise `DevSession` in a native process.
The refreshed full host-renderer workflow includes the development runtime and
WebSockets in its ON configuration and runs for stacked PRs; its new candidate
result remains pending. See the [validation handoff](../validation/pr110-merge-readiness-2026-11-10.md)
for the separate evidence sources and exact commands.

Run an actual host edit/reload session to verify state-preserving Fast Refresh,
error recovery, reconnect and native teardown before documenting those as
validated user workflows. The owner's QNX smoke report does not exercise this
host-only runtime. No QNX development-runtime support or debugger attachment
result is claimed.
