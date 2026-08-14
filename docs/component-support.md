# Component Support

What actually works, on the macOS development host, driven by a real React
Native bundle and verified by reading pixels back from a rendered frame.
Nothing in this table has been cross-compiled for QNX or run on a Raspberry
Pi 4.

Legend: ✅ works and is covered by a host test · ⚠ partial, with the limit named
· ❌ not implemented.

## Components

| Component            | Status | Notes                                                                                              |
| -------------------- | ------ | -------------------------------------------------------------------------------------------------- |
| `View`               | ⚠      | Mounts, lays out, and paints background, corner radius, and opacity.                               |
| `Text`               | ❌     | Needs the text descriptors, which need React Native's codegen run.                                 |
| `Image`              | ❌     | Needs `QQuickImageProvider` and the image manager.                                                 |
| `ScrollView`         | ❌     | `Flickable` is the intended backing.                                                               |
| `TextInput`          | ❌     | Needs Qt Quick's text input plus IME.                                                              |
| `Switch`             | ❌     |                                                                                                    |
| `ActivityIndicator`  | ❌     |                                                                                                    |
| `Modal`              | ❌     |                                                                                                    |
| `FlatList`/`Section` | ❌     | Upstream JavaScript over `ScrollView`; blocked on `ScrollView`.                                    |
| `Pressable`/`Button` | ⚠      | Touch events reach JavaScript; the responder negotiation `Pressable` relies on is not implemented. |

## `View` props

| Prop                                    | Status | Notes                                                                                                            |
| --------------------------------------- | ------ | ---------------------------------------------------------------------------------------------------------------- |
| Layout (Yoga: flex, padding, position…) | ✅     | Applied as absolute geometry; Yoga is the only layout authority.                                                 |
| `backgroundColor`                       | ✅     | Packed to 0xAARRGGBB and set as `Rectangle.color`.                                                               |
| `opacity`                               | ✅     | `QQuickItem::setOpacity`.                                                                                        |
| `borderRadius`                          | ⚠      | Qt Quick's `Rectangle` has one `radius`; per-corner radii collapse to the top-left value.                        |
| `borderWidth`, `borderColor`            | ⚠      | Only the left edge is read, because `Rectangle.border` is a single pen.                                          |
| `transform`                             | ❌     | Qt items have a transform list; not wired up.                                                                    |
| `onLayout`                              | ❌     | The mounting layer computes the metrics but does not emit the event.                                             |
| `onTouchStart`/`Move`/`End`             | ⚠      | Single pointer, hit-tested to the deepest item. No multi-touch, no pointer capture, no JS responder negotiation. |
| Accessibility props                     | ❌     | Qt Quick's accessibility bridge is not connected.                                                                |

## Platform modules

| Module                                                  | Status | Notes                                                             |
| ------------------------------------------------------- | ------ | ----------------------------------------------------------------- |
| `Platform`                                              | ⚠      | `OS`, `select`, `Version`, and constants from the running system. |
| `NativeMicrotasksCxx`, `NativeIdleCallbacksCxx`         | ✅     | React Native's own C++ modules, via `DefaultTurboModules`.        |
| `ExceptionsManager`                                     | ✅     | Reports to the host and logs; `dismissRedbox` is an honest no-op. |
| `DeviceInfo`, `SourceCode`                              | ❌     | Generated specs exist; real values needed.                        |
| `FileReaderModule`, `SegmentFetcher`, `WebSocketModule` | ❌     | Referenced by the bundle; unused by the render path.              |
| `UIManager`                                             | n/a    | Not a TurboModule; bridgeless React Native serves it from JS.     |
