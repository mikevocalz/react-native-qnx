# Real Worklets host prototype, November 10, 2026

## Boundary and review

Default-OFF native Linux experiment using actual `react-native-worklets@0.13.1`,
not an animation imitation, a second renderer or a Forge dependency.
[ADR-0014](../adr/0014-experimental-worklets-bootstrap.md) permits only this
third-party library's private bootstrap, not generic application TurboModules.
Independent native review approved that scope and found no blocking lifetime
issue in the owner queue/frame source after corrections. It identified a
teardown mutation hazard: the adapter no longer mutates app globals and a test
freezes the proxy before destroying the host.

Source: Worklets release commit `24e415d2c036a532cf7820ff0c400e49841bda50`,
RN 0.88.1, Qt 6.11.1 and the repository's exact Hermes source. Inspected upstream
UIScheduler, JSScheduler, RuntimeBindings, RuntimeManager, proxy/initializer,
serializers, unpackers, NativeWorkletsModule.ts and public JS initialization.

Margelo api-design → build-nitro-modules → cpp pipeline:
`1e9e17be6f41c838db472d6a4b943fc26355a3be`; Callstack threading guidance:
`59e9d17446a248b21d86f078fcd0bd2d36db1ca4`. Decisions: keep the library-required
bootstrap distinct from application APIs, enforce owner delivery and cancellation,
and do not fabricate a Nitro substitute for upstream imports.

## Actual implementation

- Upstream proxy, serializers and separate Hermes UI runtime initialized by
  real public JS. The existing Qt owner queue precedes the RN CallInvoker;
  the latter is not a cross-thread dispatcher in this synchronous host.
- Qt UIScheduler subclass with foreign admission, owner execution, error relay
  and cancellation. Guarded RN delivery closes before host queue teardown.
- Actual frameSwapped observation from one explicitly attached QQuickWindow.
  Its render-thread callback samples steady-clock milliseconds (upstream
  performance.now basis), then queues delivery on the owner. QQuickWindow::update
  requests the next real frame. This is not physical presentation/vsync proof.
- Explicit errors for unsupported Bundle Mode/slow animations and absent/dead
  frame windows. No timer fallback. The single upstream coalesced frame callback
  uses the scheduler exception boundary; a throw aborts that delivery, not a
  promised general fanout service.
- Linux-only/default-OFF CMake. QNX and cross builds reject this prototype until
  lean-Hermes unpacker bytecode and the actual target ABI are qualified.
- Audited build-copy-only portability adaptations: pthread naming API; missing
  algorithm/iterator headers in EventLoop, JSISerializer and WorkletsJSIUtils.
  C++20 shared_ptr atomic free-function deprecations stay visible but nonfatal
  only in upstream SynchronizableDynamic.cpp. GCC's continued-comment warning
  likewise stays visible. Platform source warning policy is not weakened.
  Upstream MIT LICENSE is copied into the build source and recorded in notices.

## Verified tests

**4/4 native CTests passed** with Clang 19.1.7, Qt 6.11.1, RN 0.88.1 and actual
Hermes/Worklets. Production qnx and marathon Metro bundles use Node 26.10.0,
Metro 0.87.1 and the audited Worklets Babel plugin.

- `rnqnx_worklets_scheduler`: worker admission, owner execution, error boundary,
  queued cancellation, late admission and off-owner final reference release.
- `rnqnx_worklets_frames`: actual offscreen Qt frame, owner callback, window
  destruction and pending callback cancellation. Injected render-thread signal
  coverage is separate from the actual frame observation.
- `rnqnx_worklets_qnx` and `rnqnx_worklets_marathon`: native version 0.13.1,
  UI runtime identity, UI→RN result 42, actual frame callback, Bundle Mode
  rejection, expected fatal UI error through RN ExceptionsManager, reload,
  frozen proxy and queued UI/RN cancellation with external delivery counters.

Standalone scheduler also passed ASan+UBSan with leak detection disabled and
prebuilt Qt uninstrumented. This is not full-runtime sanitizer qualification.
Earlier GCC full-graph attempts stopped on upstream RN Clang assumptions; the
successful full runtime uses Clang, not permissive GCC flags. Local font-cache
warnings did not alter results. Documentation/whitespace checks pass.

## Lifecycle and reproduction

After installing the normal TurboModule binding, call
`host.setWorkletsWindow(&window)` before evaluating a Worklets bundle. The
borrowed window must belong to the Qt owner. Hidden windows naturally pause
frames. A destroyed window cancels pending frames; replacement requires a new
host. The playground attaches its window only when the feature flag is ON.

Invalidation is host-stop-only after app/surface teardown. Accepted unrelated
owner-queue completions may still drain while Hermes lives. The retained JS
proxy keeps its upstream resources alive until RN destruction, even if frozen.
Closed frame/scheduler sources cancel work; the RN gate/dead-runtime status
prevent callbacks into the stopped host. There is no public shutdown/reopen API.

```sh
cmake --preset linux-host-renderer \
  -DRNQNX_ENABLE_WORKLETS=ON -DRNQNX_BUILD_WORKLETS_TESTS=ON
cmake --build build/linux-host-renderer \
  --target rnqnx_worklets_runtime_test rnqnx_worklets_scheduler_test \
    rnqnx_worklets_frames_test -j2
ctest --test-dir build/linux-host-renderer -R '^rnqnx_worklets_' \
  --no-tests=error --output-on-failure
```

Default source is the locked node_modules package. An audited external package
can be supplied using RNQNX_WORKLETS_DIR. CI runs these actual tests.

## Still unpassed

Reanimated Fabric integration, RNGH Qt recognizers, worker-runtime qualification,
network permission policy, QNX bytecode/ABI, physical-device gestures and frame
performance remain separate gates. No device or benchmark results are inferred.
Rollback: leave RNQNX_ENABLE_WORKLETS=OFF; the previous runtime graph remains.
