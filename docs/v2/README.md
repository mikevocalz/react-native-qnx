# V2 native Qt and graphics APIs

This work starts from `main` at
`0c91be9bbb4e7cd6dabccf8cdfe441c7fa57c629`. The v2 scope includes native Qt
animation, SVG, Worklets, Reanimated, Gesture Handler, React Native WebGPU,
TypeGPU, React Native Skia **v3**, Three.js and an optional Qt Quick 3D adapter.
It preserves the existing Fabric → Yoga → Qt Quick renderer and keeps Nitro
Forge as an optional, separately installed consumer.

## What this slice exposes

| API                 | Implementation in this slice                                                                               | Application boundary                                                                               |
| ------------------- | ---------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------- |
| `QtSvgDocument`     | Inline static SVG, fit/cover/stretch, load/error events, source replacement and cleanup                    | React Fabric component at `@rn-qnx/react-native-qnx/qt`, with pinned RN-generated C++ declarations |
| `MotionController`  | Qt-native opacity and transform tracks, easing, pause/resume, cancel, interruption and lifetime control    | Public native C++; the Nitro v1 binding is still a separate delivery gate                          |
| `QtGraphicsSurface` | Qt backend, viewport/DPR, native-surface and scene-graph generations, frame-swap observations and teardown | Public native C++; it does not create a GPU device or expose native pointers to JS                 |

Enable each native capability explicitly:

```sh
cmake --preset macos-host-tests \
  -DRNQNX_ENABLE_QT_SVG=ON \
  -DRNQNX_ENABLE_QT_MOTION=ON \
  -DRNQNX_ENABLE_QT_GRAPHICS=ON
```

Use the existing platform preset/toolchain on QNX or the documented Linux host
configuration. These switches require Qt 6.11.1 and a Linux, macOS or QNX host;
SVG additionally requires `Qt6::Svg`. All three default to OFF. Their native
unit-test projects can also be configured independently under `tests/native`.
The flags do not establish third-party package compatibility or device support.

- [Render SVG from React](svg-component-api.md)
- [Static SVG subset and native rendering contract](svg-documents.md)
- [Render `react-native-svg` elements](react-native-svg.md) (stage 4)
- [Native motion ownership and operations](native-motion.md)
- [Graphics backends and the Rust-engine adapter proposal](graphics-backends.md)
- [Validation and source evidence](validation.md)

## Required library adapters

The following order groups work by concrete dependencies. Each stage needs its
own generated/native contract, failure behavior and real runtime test. A
successful earlier stage does not mark a later one supported.

| Stage                          | Deliverable                                                                                                                | Acceptance before enabling in an app                                                                                                                                                                                                                              |
| ------------------------------ | -------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1. Native Qt foundation        | This slice's document, motion and surface APIs                                                                             | Host runtime tests; QNX/Marathon target rows remain separate                                                                                                                                                                                                      |
| 2. Runtime and frame ownership | RN JS owner queue, UI-worklet runtime, runtime factory, frame callbacks, transfer and shutdown                             | An actual transformed worklet runs off the RN JS owner, survives cancellation/reload, and never touches Qt objects from the render thread                                                                                                                         |
| 3. Reanimated and gestures     | Real upstream bootstraps, Fabric prop/measure/scroll hooks, recognizer registry and arbitration                            | Drag plus spring under JS load; simultaneous/wait-for/failure/cancel semantics; hit testing follows presentation geometry                                                                                                                                         |
| 4. SVG elements                | `react-native-svg` descriptors, retained element props, inheritance, defs/gradients, per-element events and animated props | Upstream component fixtures plus rendered pixels, updates and touch targets; SVG document support alone is insufficient. Implemented behind `RNQNX_ENABLE_QT_RNSVG` ([guide](react-native-svg.md)); animated props are JavaScript-driven until stage 2 and 3 land |
| 5. QNX graphics backend        | Pinned Dawn/Tint build, native driver/adapter, limits/features, surface/composition and device-loss recovery               | Native clear/draw/readback and resize/recreate on the actual target; no assumed Pi Vulkan support. Ran on the Pi 400 over OpenGL ES; see [WebGPU on QNX](webgpu-qnx.md)                                                                                           |
| 6. GPU libraries               | RN WebGPU → TypeGPU/Three.js; Skia v3 Graphite using the compatible Dawn closure                                           | Render and compute fixtures, native presentation, images/fonts/assets, interop and teardown on target                                                                                                                                                             |
| 7. Optional engine adapter     | A licensed Rust engine's C ABI feeding the shared Dawn/Skia renderer                                                       | Native buffers/commands, measured transfer costs, matching device ownership and no dependency from platform core to Forge                                                                                                                                         |
| 8. Optional Qt 3D              | Qt Quick 3D scene/camera/material/model/picking components                                                                 | Device rendering/input and module-specific distribution terms; no Three.js compatibility claim                                                                                                                                                                    |

The existing [input and Worklets plan](../platform-modernization/02-input-worklets.md)
and [exact-source audit](../reanimated-gesture-handler-qnx-audit.md) remain the
input/runtime audit. Stages 2 and 3 are implemented behind default-OFF switches
in [Worklets](../worklets-qnx.md) and
[Reanimated and Gesture Handler](../reanimated-gesture-handler-qnx.md); those
pages say what ran on the device and what did not.
This slice does not change the React Native, Hermes or Nitro versions, install
the candidate third-party libraries, or turn the existing QTimer-based core
Animated implementation into Reanimated.

## Thread and property authority

The inspected host constructs one Hermes runtime and invokes its runtime
executor inline. Moving RN execution to a dedicated JS owner remains necessary
before claiming that UI worklets avoid a busy RN thread. Creating another
runtime on the same busy thread is not that separation.

The intended owners are:

| Owner                           | Responsibility                                                              |
| ------------------------------- | --------------------------------------------------------------------------- |
| RN JS queue                     | React execution, module calls and JS completions                            |
| Qt GUI/UI-worklet owner         | Qt item mutations, native input and per-frame UI calculations               |
| Qt render thread, where present | Scene-graph/render-resource work in Qt's allowed phases                     |
| Optional engine worker          | Document algorithms/CPU work, with explicit native transfer to the renderer |

Worklets serialize eligible JavaScript and captured values into another runtime;
they are not Qt QML functions or C++ compilation. Use the selected upstream
Worklets implementation and transformer. QNX's lean-Hermes bytecode path and
runtime module loading must be qualified together.

`QtGraphicsSurface::frameSwapped` is an observation after a Qt signal reaches
the GUI owner. It is unsuitable as the same-frame UI-worklet scheduling hook.
A frame scheduler must define its actual Qt phase, monotonic timestamp units,
request/cancel behavior, hidden-window policy, multiple-window ownership and
runtime teardown. No fixed 16 ms timer can establish display synchronization.

Native motion owns its target properties while running. A future binding must
arbitrate Fabric commits, direct Reanimated writes, ordinary Qt animations and
imperative motion. It must also resolve a view by surface/tag plus lifetime,
never by a recycled numeric tag alone. The native transform layer preserves
Yoga geometry; integration still needs explicit hit-testing and measure rules.

## Binding policy

The SVG view uses Fabric's component contract and genuine RN 0.88.1 codegen.
Application-facing service/resource APIs use Nitro v1's pinned HybridClass
declarations, registration and constructors. The constrained PlatformInfo
generator does not already support a motion resource, callback subscription or
GPU buffer graph. Extend generation with negative tests and prove it through
Hermes before shipping those JS APIs. Proposed TypeScript in the native guides
is not an installed module.

Worklets, Reanimated, Gesture Handler, RN WebGPU and Skia keep their real
upstream initialization contracts. A generic Nitro wrapper does not implement
their JSI objects, Fabric hooks or renderer behavior. Record any required
framework integration exception through the existing ADR process.

## Graphics and Rust decision

Skia v3 uses Graphite over Dawn; RN WebGPU uses Dawn too. The preferred optional
Rust integration feeds that same native device through a narrow C ABI/C++
adapter. Rust retains the document and algorithms. The adapter owns renderer
resources, submissions and completion. CPU-generated buffers still need their
explicit transfer/upload path. Direct Rust bindings to the same pinned Dawn C
API are possible later; independent Rust `wgpu` handles are not Dawn handles.

Qt Quick remains the React Native UI renderer. WebGPU/Skia content must compose
into it through a qualified surface/texture path with correct synchronization,
color/alpha conventions, resize and loss handling. Qt Quick 3D is a separate
optional native scene route, not the implementation behind Three.js. Details
and primary sources are in [graphics-backends.md](graphics-backends.md).

## Rollback

Turn the three `RNQNX_ENABLE_QT_*` switches OFF and remove the experimental
component import from consuming apps. No persisted format, existing Metro
alias, platform identity or standard RN API changes. Reverting the slice also
removes its additive package entry and generated sources; the optional Forge
consumer remains independent.
