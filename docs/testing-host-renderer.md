# Current-line full host renderer validation

The ordinary `test:cpp:host` lane remains a fast smoke lane on machines without
Qt. A successful smoke run does **not** validate the renderer. The separate
`SDK 58 full host renderer` workflow requires Qt 6.11.1, the frozen SDK 58 / React
Native 0.88.0-rc.3 dependencies, and Hermes source commit
`4188b63f24026ca62b79dc81d95db122a59c6089` selected by that RN release.

On Linux with Clang (using the system libstdc++ ABI), CMake 3.31.9+, Ninja, OpenGL/EGL development packages,
Python, ICU development headers/libraries, and Qt 6.11.1 Core/Gui/Qml/Quick installed:

```sh
pnpm install --frozen-lockfile
export CMAKE_PREFIX_PATH=/path/to/Qt/6.11.1/gcc_64
node scripts/validate-host-renderer.mjs
```

First configuration fetches the pinned native dependencies and Hermes source.
The build uses two parallel jobs to stay within ordinary CI runner memory.
There is no warm-cache requirement. The strict CMake option rejects missing Qt,
disabled Hermes, disabled dependency fetching, or disabled tests. The runner
also rejects any missing test from its nine-test inventory before executing
CTest. The dedicated workflow saves the inventory, JUnit and CTest/configure
logs under an artifact named for the run's exact commit.

Coverage includes the actual Hermes runtime, installed RN headers, Qt items,
Fabric mutation conversion and ShadowTree mounting, plus the production Metro
playground bundle through React, Hermes, Yoga and Qt. The QtReactHost suite also
checks painted output, JS events, text inputs, lists, animation and lifecycle
behavior. It runs headlessly with Qt's software backend. Existing ASan/UBSan
instrumentation is enabled; third-party source targets are not all instrumented.

This is Linux host evidence only. It does not establish a licensed QNX SDP
8.0.5 cross-build, QNX Screen/QPA behavior, a physical Pi result, or on-device
performance. The known main-branch dependency audit fix remains separate in
PR #27; this lane does not suppress that audit.

## Native source seams used by this lane

- React Native's trimmed Boost 1.84.0 archive only includes
  `boost/config/stdlib/libcpp.hpp`. GCC's standard library selection in
  `boost/config/detail/select_stdlib_config.hpp` requires `libstdcpp3.hpp`.
  `cmake/dependencies/ReactNativeThirdParty.cmake` supplies the missing
  configuration headers from hash-pinned official
  [Boost.Config 1.84.0](https://github.com/boostorg/config/tree/boost-1.84.0/include/boost/config).
  The trimmed Boost implementation and its version remain unchanged.
- RN's generated `React/FBReactNativeSpec/FBReactNativeSpecJSI.h` contains Apple
  `#pragma mark` directives. Its installed include roots are marked SYSTEM,
  matching the existing treatment of RN's other headers, so upstream pragmas
  do not become project `-Werror` failures on GCC.
- The exact Hermes `CMakeLists.txt` checks `ICU_FOUND` even with Intl disabled.
  The Linux lane installs ICU development headers/libraries explicitly.
- RN 0.88 `ReactInstance::createJSCallInvoker()` still uses
  `RuntimeSchedulerCallInvoker` in its non-buffered compatibility path and
  narrowly suppresses that type's deprecation. This custom single-thread host
  retains the same path with the same local diagnostic scope; it does not
  claim the buffered callable-module ordering of a full `ReactInstance`.
- The current RN target graph also needs upstream `react/cxxstableapi`,
  `jserrorhandler`, `react/renderer/observers/resize` and
  `react/nativemodule/resizeobserver`. These are built from their shipped
  CMakeLists rather than empty alias targets.
- `react/debug/redbox/AnsiParser.h` and inspector `network/HttpUtils.h` omit
  `<cstdint>` for their integer types. The build supplies that standard header
  to those two translation units. RN's compiled
  sources retain nonfatal GCC diagnostics for Apple-only pragmas; first-party
  code and tests retain strict warnings-as-errors.
- RN 0.88's feature-flag accessor uses 16-byte atomic state. The Linux graph
  links `libatomic` explicitly for the emitted `__atomic_*_16` operations.

## Runtime regressions exposed by the full lane

The suite exposed two real event-loop/input bugs, plus outdated QML fixtures:

- `UIManagerBinding::completeRoot` now queues asynchronous rendering updates.
  Source evaluation, native timer callbacks, and UIManager's AppRegistry
  start/update/stop executor enter a real `RuntimeScheduler::scheduleWork` task, so its microtask and rendering
  checkpoints run. Direct JSI evaluation left manual Fabric commits and timed
  React state updates pending. The synchronous host rejects reentrant source
  evaluation instead of letting references escape into a later task.
- Mouse release now reports its ending contact only in `changedTouches`.
  `touches` and `targetTouches` are empty, matching the existing multi-touch
  path and React's responder-release contract.
- The test View aliases `backgroundColor` to Rectangle's paint color, matching
  the production View wrapper's style contract. The test Image uses the same
  loading-indicator overlay as the playground. Existing color, pixel, loading,
  responder and virtualized-list assertions remain enabled.

Additional regression cases cover rejected reentrant evaluation without side
effects, timer Promise microtasks completing without a manual drain, and a real
Metro-bundled React `useState` update changing Yoga/Qt geometry from 64 to 96.
