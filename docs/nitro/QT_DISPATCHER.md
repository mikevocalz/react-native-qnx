# Owned Qt/JavaScript Nitro v1 dispatch

The opt-in QNX dispatcher runs Nitro callbacks and Promise settlement through
the existing Qt event loop and React Native RuntimeScheduler. Physical Pi 400
and host results are in [the receipt](../evidence/pi400-nitro-qt-2026-11-07.md).
This is a runtime integration experiment on pinned alpha 136, not a Forge
JavaScript binding, an ABI upgrade or general third-party module support.
Audited on 105; re-pinned to 136 with `Dispatcher.hpp`/`Callback.hpp` byte-identical; QNX cross-build and device not re-run.

## Ownership and execution

JavaScript currently shares the constructing Qt thread; this does not add a
separate JS thread. Native engines must keep heavy work on their own workers.
`QtReactHost::taskQueue()` provides the shared native completion route:

```cpp
auto dispatcher = std::make_shared<rnqnx::nitro::QtNitroDispatcher>(host.taskQueue());
rnqnx::nitro::installQnxNitroRuntime(host.runtime(), dispatcher);
// Install once, before importing any Nitro-backed JavaScript.
```

Worker `runAsync` posts an owned task to the host's QObject, then the owner
enters an RN scheduler task. That boundary performs RN's microtask checkpoint
and rendering update. `runSync` requires the owner thread and throws otherwise;
it never blocks a worker waiting for Qt. Nitro `Callback::callSync` bypasses
`runSync`, so dispatcher JS-entry hooks also track nested entries and checkpoint
the outermost direct native call. Errors from checkpoints use the existing
host error reporter; awaited callbacks/Promises retain their failure paths.

The admission mutex protects the pending list, closed state and QObject receiver
against concurrent posting/close. Qt QueuedConnection posts a wakeup without
executing user work. All task execution and accepted-capture destruction occur
on the owner, outside that mutex. Nested Qt pumping does not reorder the FIFO.

At idle owner-thread destruction, the host first stops and unregisters surfaces
while callbacks remain available for cleanup. It then closes worker admission
before scheduler or Hermes destruction. It drains every accepted task,
including owner-thread settlement tasks posted transitively during draining.
Then it deletes the QObject and clears the closures that borrowed host state.
Retained queues reject late work without retaining Hermes. Active-JS/reentrant
host destruction is invalid; dispose native producers at an idle owner boundary.

Close is a drain, not cancellation or a deadline. A task that never returns or
continually posts more owner work can delay it. Do not join workers that need
synchronous JS calls from the JS owner. Forge worker shutdown and HybridClass
disposal still need their own binding-level integration and tests.

## Pinned Nitro compatibility

`NitroDispatcherPatches.cmake` requires the exact alpha 136 source structure.
Audited on 105; re-pinned to 136 with `Dispatcher.hpp`/`Callback.hpp` byte-identical; QNX cross-build and device not re-run.
The patches:

- `Dispatcher::runAsyncAwaitable` catches admission failure and rejects its native
  Promise. Without this, a closed queue could throw into a native caller.
- `Callback::callAsyncAndForget` catches failed admission and uses the existing
  Nitro GlobalErrorHandler. That handler's logging remains debug-only, matching
  pinned upstream behavior; use an awaited callback when the caller needs errors.

Accepted opaque awaitable closures are never silently discarded: doing so would
leave their returned native Promise pending. No Nitro ABI/layout or package
versions change. Apply transformations only to an isolated dependency copy.
The host build rejects its installed canonical core as a patch destination.
The patch-contract test verifies fresh application, repeated application,
unchanged installed headers and rejection of simulated source drift.

## Build and run

Keep the existing licensed SDP/Qt/Hermes/RN prerequisites. In addition to the
normal full-host or QNX options, pass:

```bash
-DRNQNX_ENABLE_NITRO_QT_DISPATCHER=ON \
-DRNQNX_NITRO_CORE_ROOT=/absolute/isolated/nitro-core \
-DRNQNX_NITRO_ROOT=/absolute/react-native-nitro-modules
```

The flag defaults OFF and requires the full Qt/Hermes React host. QNX also
requires `RNQNX_BUILD_NITRO_QNX=ON`. The audited core version is exactly
`1.0.0-alpha.nightly.136.gdb81de61bd32`; unsupported OS/configurations fail.
The existing no-dispatcher installer remains available for synchronous probes.

For QNX, also supply `RNQNX_NITRO_TEST_HERMESC` pointing to the build host's
compiler from pinned `hermes-compiler@260318099.0.5`. Build:

```bash
cmake --build /absolute/build --parallel 4 \
  --target rnqnx_nitro_qt_test rnqnx_playground_qnx
```

Lean QNX Hermes consumes bytecode. CMake compiles the six shared test fixtures
under `tests/fixtures/nitro-qt` to `build/nitro-qt-fixtures/*.hbc`. Deploy them
with the test executable and the same private Qt/Hermes runtime as Hello World.
Set `RNQNX_NITRO_QT_FIXTURES` to their target directory before running the probe
as qnxuser. It opens no application window. Use an external process watchdog.
Host tests read the same source fixtures and run through CTest:

```bash
ctest --test-dir /absolute/full-host-build --output-on-failure
```

With host sanitizers enabled, the new queue translation unit, Qt dispatcher and
test are instrumented with ASan/UBSan. Third-party Nitro, RN, Hermes and Qt are
not thereby instrumented; leak checking remains disabled for the existing host
singleton policy. No QNX sanitizer or TSan result is claimed.

The private queue uses a list rather than RN's deque specialization. Mixing
instrumented deque annotations with RN's uninstrumented instantiation produced
an ASan container-annotation conflict in existing renderer tests. Separating
that specialization restored the full suite without disabling ASan checks.

## Boundaries and rollback

The physical test ran on alpha 105 and covers typed/void Promise resolution, rejection, callback
errors, checkpoints, accepted-work draining, owner-thread capture release,
transitive JS settlement and 3,000 posts racing teardown. It does not establish
listener cancellation, reload, suspend/resume, third-party Nitro APIs, QNX
Nitrogen generation or Forge alpha 120/alpha 136 compatibility. No frame, startup
or memory budget was measured. Marathon's installer is unchanged and was not
runtime-tested by this QNX lane.

Disable the flag to use the previous synchronous installer. No boot, graphics,
window-size/orientation, dependency-pin or application-state migration is needed.
