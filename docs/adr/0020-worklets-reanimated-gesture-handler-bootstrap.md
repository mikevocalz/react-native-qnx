# ADR-0020: Native bootstrap for Worklets, Reanimated and Gesture Handler

## Status

Accepted, 2026-11-13, under the owner's instruction to deliver stages 2 and 3
of the v2 plan. Worklets has a device run; the Reanimated and Gesture Handler
parts are recorded in [worklets-qnx.md](../worklets-qnx.md) and
[reanimated-gesture-handler-qnx.md](../reanimated-gesture-handler-qnx.md) with
the results that actually ran.

## Context

[ADR-0004](0004-nitro-modules.md) allows a minimal adapter for React Native
core's own registry requirements and sends application APIs to Nitro. Three
third-party libraries sit outside both:

- `react-native-worklets` looks up `TurboModuleRegistry.get('WorkletsModule')`
  and calls `installTurboModule`, `start`, `prepareBundleMode` and
  `toggleSlowAnimationsOnUIRuntime`. The native side then installs
  `global.__workletsModuleProxy` itself.
- `react-native-reanimated` looks up `ReanimatedModule` and expects
  `global.__reanimatedModuleProxy`.
- `react-native-gesture-handler` looks up `RNGestureHandlerModule`.

Their JavaScript is upstream's and is not rewritten. A Nitro HybridObject cannot
answer those lookups, and an empty TurboModule would hide the failure
ADR-0004 asks us to keep visible.

## Decision

The platform registers these three modules as named TurboModules through
`QtReactHost::registerTurboModule`, behind default-OFF switches
(`RNQNX_ENABLE_WORKLETS` and its siblings). Each module is a thin bootstrap: it
forwards to the upstream C++ (`WorkletsModuleProxyInitializer` and so on),
which is compiled from the installed npm package and not vendored. The module
returns an error for anything the platform does not implement; it never returns
a stub that reports success.

Application-facing APIs stay on Nitro. The three registrations are the only
third-party TurboModule names the platform provides, and a new name needs its
own ADR.

The host also gets `addTeardownHook`, so a library that owns a thread or a
runtime stops it while Hermes is alive instead of relying on destructor order.

## Consequences

- Upstream JavaScript runs unmodified. The Babel plugin, `scheduleOnUI`,
  `useSharedValue` and the gesture hooks keep their published behavior.
- The platform carries the upstream version constraint. CMake rejects a
  Worklets outside 0.13.x.
- Each patch to upstream C++ is a one-line text substitution in CMake that fails
  the configure step if the upstream line changes.
- Bundle Mode, sensors, keyboard events and pseudo-selector callbacks are not
  provided; they report that on use.

## Alternatives considered

- A Nitro module that re-exports the library: rejected, the library's JSI proxy
  is installed by the library, and Nitro cannot satisfy the registry lookup.
- Forking the libraries: rejected, it forfeits upstream fixes and the Babel
  plugin contract.
- Empty TurboModules: rejected by ADR-0004.

## Related files

- `vnext/src/worklets/`, `vnext/include/react-native-qnx/worklets/`
- `cmake/RnqnxWorklets.cmake`
- `vnext/src/runtime/QtReactHost.cpp` (`registerTurboModule`, `addTeardownHook`)
