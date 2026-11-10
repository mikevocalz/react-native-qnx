# Qt frame-swap signal capture

This adds capture plumbing to the existing default-OFF
`RNQNX_ENABLE_FRAME_TRACE` experiment. No readiness gate is promoted and no
performance result, physical frame presentation, FPS, RSS or first-contentful
frame measurement is claimed.

## Recorded boundary

The playground's existing `--trace-json <new-path>` session now observes its
`QQuickWindow::frameSwapped` signal. The bounded recorder exports an instant
`qt.frame-swapped` event on the emitting thread. Qt 6.11.1 defines this signal
as a frame **queued for presenting**, emitted by the scene graph rendering
thread. It is not a receipt from the display controller or physical panel.
The software renderer also emits this signal; host offscreen results do not
qualify QNX Screen or a physical display.

Timestamps remain session-relative `std::chrono::steady_clock` record-copy
samples taken after the recorder lock. They are not Qt's timestamp or calibrated
to QNX/display clocks. The export includes `frameSwapMeaning` to preserve this
boundary with the artifact. Surface and surface-generation fields are zero:
a window swap must not be attributed to a particular React commit/surface.
Correlation IDs distinguish observations; existing sequence and dropped-record
fields retain their meaning. Trace capacity and the bounded thread table are
unchanged. Truncation or recorder contention can invalidate timing analysis.

## Ownership and source audit

`QtFrameSwapTrace` is an internal, noncopyable scoped connection, not a new
JavaScript API, Nitro wrapper, renderer or scheduler. Construct/destroy it on
the window's owner thread. A `Qt::DirectConnection` records in the actual signal
callback without queued GUI work. The lambda captures only the shared recorder;
it touches no window, Qt item or JS runtime and performs no export or file I/O.
The existing recorder lock protects its cross-thread copy, with no callbacks
under the lock. Disconnect prevents new callbacks; an in-flight callback can
finish safely using the retained recorder. Sender destruction disconnects
and releases the capture. Freeze rejects any later recording.

The playground disconnects the observer after the host workload and before
freezing/exporting, using the same output-path and owner-only file permissions
as the existing trace export. Without `--trace-json` there is no signal
connection. With the feature flag OFF there is no new target or signal hook.

Inspected upstream Qt `qt/qtdeclarative` tag `v6.11.1`:
`src/quick/items/qquickwindow.cpp` (`frameSwapped` contract) and
`src/quick/scenegraph/adaptations/software/qsgsoftwarethreadedrenderloop.cpp`
(`fireFrameSwapped` after rendering/flush). Existing RuntimeTrace ownership,
bounds and export contracts were inspected in this repository.

Pinned Margelo `api-design`, `build-nitro-modules`, and `cpp` guidance at
`1e9e17be6f41c838db472d6a4b943fc26355a3be` informs the scoped cleanup handle,
framework-boundary naming, and RAII ownership. This internal recorder does not
add an application-facing native API, so no Nitro generation or ABI is changed.
The repository native/API and platform-validation skills apply; source review
is not installation, compilation or hardware qualification.

## Tests and usage

`QtFrameSwapTraceTest.cpp` covers deterministic injected signals on GUI/worker
threads, sequence/export semantics, scoped disconnect, freeze, null sessions,
and sender-first destruction. A separate case opens a real offscreen Qt Quick
window and requires an actual `frameSwapped` signal within a bounded deadline.
Injected signals are not rendering proof. The test registers only with tracing
ON in the existing Qt host graph. The host validation script requires its
registration when tracing is enabled. Core RuntimeTrace tests also cover the
new stage's JSON export without requiring Qt.

For the matching Qt host environment:

```sh
cmake --preset linux-host-renderer -DRNQNX_ENABLE_FRAME_TRACE=ON
node scripts/validate-host-renderer.mjs
```

For a later authorized device capture, cross-build the same flag and use the
existing playground `--trace-json` option with a new destination. Archive exact
revision, build flags, binary/bundle hashes, board/image, Qt/QPA/backend,
display mode and raw trace. Test lifecycle/shutdown and capture overhead on that
target before using intervals. This change does not run a device or deployment.

Rollback: disable `RNQNX_ENABLE_FRAME_TRACE`, or revert this capture slice.

## Local validation receipt

Linux x86_64, GCC 14.2.0, CMake 3.31.10, official Qt 6.11.1 Linux binaries and
ICU 73. The isolated Qt fixture includes the actual `RnqnxRuntimeTrace.cmake`
and `RnqnxFrameSwapTrace.cmake` modules; it does not compile the full
React/Hermes/playground graph.

- Strict-warning core recorder build/run: passed.
- OFF header compile/run without Qt: passed; no Qt type exposed when disabled.
- `cmake --build build/frame-swap-review/cmake --parallel 2`, then
  `ctest --test-dir build/frame-swap-review/cmake --output-on-failure`:
  **3/3 passed**, including the actual offscreen window signal.
- Same targeted graph with `RNQNX_ENABLE_HOST_SANITIZERS=ON` and
  `ASAN_OPTIONS=detect_leaks=0`: **3/3 passed**. Recorder, helper and test
  code are instrumented; official Qt binaries are not. LeakSanitizer cannot
  run under this workspace's ptrace environment, so leak detection is not a pass.
- JSON export parsed successfully; `frameSwapMeaning` preserves the limitation.
- Full host renderer/playground integration, QNX cross-build, physical target,
  hardware presentation and capture-overhead measurements: **not run here**.

Callstack `react-native-best-practices` at
`59e9d17446a248b21d86f078fcd0bd2d36db1ca4` was read: this implements bounded
measurement plumbing, not a claimed optimization without baseline/re-measurement.
- With `BUILD_TESTING=OFF`, the Qt observer library builds and the
  `rnqnx_qt_frame_swap_trace_test` target is absent. The existing core-recorder
  target policy is unchanged. Both normal and instrumented 3-test graphs were
  rerun successfully after this target guard was added.
