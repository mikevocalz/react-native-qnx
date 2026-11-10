# Host development runtime slice (default OFF, unvalidated)

Status: source implementation only. Nothing in this slice has been compiled, run, or tested. Host-only; QNX is rejected.

## Protocol and source references

- React Native 0.88.1 `Libraries/Core/setUpReactRefresh.js`: Refresh hook injected before the renderer; requires `DevSettings.reload`.
- React Native 0.88.1 `Libraries/Utilities/DevSettings.js`: `reloadWithReason` and `onFastRefresh` delegate to native `DevSettings`.
- metro-runtime 0.87.1 `src/modules/HMRClient.js`: `update` injects `added`/`modified` through `globalEvalWithSourceUrl(code, sourceURL)`.
- Metro 0.87.1 `HmrServer.js`: `register-entrypoints` with the exact bundle URL; replies `bundle-registered`, `update-start`, `update`, `update-done`, `error` (`GraphNotFoundError`, `RevisionNotFoundError`).
- Hermes 9a115f15785851f4ff2747ebb090ddd066fc8ad2: the lean backend cannot compile source (`HBCStub.cpp`), so development needs `hermesvm`.

## Lifecycle

`DevSession` (owner Qt thread) fetches the `dev=true&hot=true&platform=qnx` bundle asynchronously with a deadline, byte limit and bounded retries. Every start/reload/stop increments the runtime generation; stale replies and reports are dropped. Reload waits for the host's JS idle boundary, calls `__RNQNX_DEV_DISPOSE__`, destroys the host (closing its task queue), and creates a new one on the same window and mounting layer. Metro reconnects use a separate connection generation with capped backoff. Graph loss requests one full reload.

Run: `rnqnx_playground_host --dev-bundle-url <url>` (always fullscreen; F5/Ctrl+R reload resets state).
