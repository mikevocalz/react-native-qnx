# Native application lifecycle observation

This isolated, default-OFF Qt/C++ experiment observes the current
`QGuiApplication` state and subsequent `applicationStateChanged` signals.
It does not yet install a React Native AppState module or a Nitro binding.
Existing runtime targets and boot behavior are unchanged.

Configure with the selected Qt toolchain:

```bash
cmake -S native/lifecycle -B build/lifecycle -G Ninja \
  -DRNQNX_BUILD_LIFECYCLE_OBSERVER=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/lifecycle
ctest --test-dir build/lifecycle --output-on-failure
```

Create `QtApplicationLifecycle` on the application owner thread, connect its
`stateChanged` signal, then read `snapshot()` synchronously on that thread.
Snapshots contain observed visibility, a sequence counter and a closed flag.
Active, inactive, hidden and suspended remain distinct; unrecognized values
are unknown. Duplicate observations do not emit another change. Reentrant
transitions are delivered in increasing sequence order. Consumers must not
move the observer to another thread or throw through Qt signal delivery.

`close()` disconnects future observations and is idempotent. It also runs on
`aboutToQuit`. QObject ownership disconnects a destroyed observer. Already
queued receiver deliveries are not recalled; downstream bindings must reject
stale generations when their runtime closes. Native observation alone does
not authorize background work, quiesce producers, persist data, implement
Marathon freeze/thaw, or guarantee the OS reports every state transition.

The dedicated CI compiles real Qt code and exercises its signal path using
synthetic lifecycle inputs. ASan/UBSan instrument this observer/test code, not
Qt's prebuilt libraries. Leak detection is disabled for Qt process-global
allocations, so no leak-free claim follows. Linux offscreen tests are not QNX
cross-builds, physical-device lifecycle receipts or Marathon shell tests.
A QNX toolchain can select the library with `BUILD_TESTING=OFF`; that path
still requires independent compile/runtime qualification.

This is Phase 4's native provider prerequisite. The next slice must connect
it to supported generated Nitro v1 or framework-internal AppState contracts,
using the existing runtime scheduler and teardown ownership, not a second
bridge. Follow `qnx-native-api`, `qnx-platform-validation` and the actual
Nitro v1 policy. Core tests require no premium modules. Revert this directory
and its isolated workflow to remove the experiment.

Sources: [Qt application state](https://doc.qt.io/qt-6/qguiapplication.html#applicationStateChanged)
and [Qt object threading](https://doc.qt.io/qt-6/qobject.html#thread-affinity).
