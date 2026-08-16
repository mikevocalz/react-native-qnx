# Component Support

What actually works, on the macOS development host, driven by a real React
Native bundle and verified by reading pixels back from a rendered frame.
Nothing in this table has been cross-compiled for QNX or run on a Raspberry
Pi 4.

Legend: ✅ works and is covered by a host test · ⚠ partial, with the limit named
· ❓ untested, no claim either way · ❌ not implemented.

## Components

| Component                  | Status | Notes                                                                                                                                                                                                                                                                                                                                                                         |
| -------------------------- | ------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `View`                     | ⚠      | Mounts, lays out, and paints background, corner radius, and opacity.                                                                                                                                                                                                                                                                                                          |
| `Text`                     | ⚠      | Renders with string, font size, colour, mixed styling, and wrapping. No ellipsis or inline attachments.                                                                                                                                                                                                                                                                       |
| `Image`                    | ✅     | Renders from `data:` URIs through this platform's image provider. `onLoadStart`, `onLoad`, `onLoadEnd`, and `onError` events are wired from QML `Image` status changes. Bundler assets, `defaultSource`, `getSize`, and `prefetch` are still missing. Covered by a host test.                                                                                                 |
| `ScrollView`               | ⚠      | Mounts as a Qt `Flickable` that clips and scrolls. `onScroll` carries `contentOffset`, `contentSize` and `containerSize`. No `contentContainerStyle`, no momentum/begin/end events, no `scrollTo`, `zoomScale` is always 1.                                                                                                                                                   |
| `TextInput`                | ✅     | Renders through the QNX platform override: `defaultValue`/`value`, `onChange`, and `onChangeText` all reach JavaScript, with `mostRecentEventCount` tracked for controlled edits. `text`, font size, colour, `editable`/`readOnly`, `secureTextEntry`, and `maxLength` apply. No placeholder, focus/blur/submit/selection events, `focus`/`blur` commands, multiline, or IME. |
| `Pressable`                | ✅     | Renders and `onPress` fires from a real pointer press; covered by a host test.                                                                                                                                                                                                                                                                                                |
| `FlatList`                 | ✅     | Renders and virtualises: the window follows the scroll and off-window cells are released. `onLayout` and `onContentSizeChange` both reach it. Covered by a host test.                                                                                                                                                                                                         |
| `VirtualizedList`          | ✅     | Covered transitively — `FlatList` is a thin wrapper over it, and the virtualisation asserted in that test is this component's.                                                                                                                                                                                                                                                |
| `SectionList`              | ❓     | Untested. Upstream JavaScript over `VirtualizedList`, which works, so it is a candidate for costing nothing.                                                                                                                                                                                                                                                                  |
| `VirtualizedSectionList`   | ❓     | Untested, same reasoning as `SectionList`.                                                                                                                                                                                                                                                                                                                                    |
| `Button`                   | ❓     | Untested. Composes `View`, `Text` and a touchable, all of which work.                                                                                                                                                                                                                                                                                                         |
| `TouchableOpacity`         | ❓     | Untested. Built on the responder system, which the `Pressable` test exercises.                                                                                                                                                                                                                                                                                                |
| `TouchableHighlight`       | ❓     | Untested, same reasoning as `TouchableOpacity`.                                                                                                                                                                                                                                                                                                                               |
| `TouchableWithoutFeedback` | ❓     | Untested, same reasoning as `TouchableOpacity`.                                                                                                                                                                                                                                                                                                                               |
| `ImageBackground`          | ❓     | Untested. Composes `View` and `Image`.                                                                                                                                                                                                                                                                                                                                        |
| `KeyboardAvoidingView`     | ❓     | Untested, and depends on the `Keyboard` native module, which is not implemented.                                                                                                                                                                                                                                                                                              |
| `ActivityIndicator`        | ✅     | Registered, mounts, and renders as a Qt Quick circular spinner. `animating`, `color`, `hidesWhenStopped`, and `size` apply. Covered by a host test.                                                                                                                                                                                                                           |
| `RefreshControl`           | ❌     | Same: `PullToRefreshViewComponentDescriptor` is already compiled. Also needs `Flickable` pull-to-refresh behaviour.                                                                                                                                                                                                                                                           |
| `Modal`                    | ❌     | React Native ships a portable `modal/platform/cxx` descriptor. Needs a second Qt window or an overlay item.                                                                                                                                                                                                                                                                   |
| `SafeAreaView`             | ✅     | `SafeAreaViewComponentDescriptor` registered and mounts as a `View`. Insets are zero on QNX. Covered by a host test.                                                                                                                                                                                                                                                          |
| `Switch`                   | ✅     | Renders and toggles as a QML switch; `value`, `disabled`, `onTintColor`, `thumbTintColor`, `tintColor`, and `onValueChange` all work. Covered by a host test.                                                                                                                                                                                                                 |
| `StatusBar`                | ❌     | There may be nothing to control on a QNX target; decide whether this is a no-op before implementing.                                                                                                                                                                                                                                                                          |

Android- and Apple-only exports are out of scope for this platform:
`DrawerLayoutAndroid`, `ProgressBarAndroid`, `TouchableNativeFeedback`, and
`InputAccessoryView`.

Counting the 22 components above: 7 work to some degree, 9 are untested
compositions over parts that already work, and 6 need native work.

## `View` props

| Prop                                                                                    | Status | Notes                                                                                        |
| --------------------------------------------------------------------------------------- | ------ | -------------------------------------------------------------------------------------------- |
| Layout (Yoga: flex, padding, position…)                                                 | ✅     | Applied as absolute geometry; Yoga is the only layout authority.                             |
| `backgroundColor`                                                                       | ✅     | Packed to 0xAARRGGBB and set as `Rectangle.color`.                                           |
| `opacity`                                                                               | ✅     | `QQuickItem::setOpacity`.                                                                    |
| `borderRadius`                                                                          | ⚠      | Qt Quick's `Rectangle` has one `radius`; per-corner radii collapse to the top-left value.    |
| `borderWidth`, `borderColor`                                                            | ⚠      | Only the left edge is read, because `Rectangle.border` is a single pen.                      |
| `transform`                                                                             | ❌     | Qt items have a transform list; not wired up.                                                |
| `onLayout`                                                                              | ❌     | The mounting layer computes the metrics but does not emit the event.                         |
| `onTouchStart`/`Move`/`End`                                                             | ⚠      | Single pointer, hit-tested to the deepest item. No multi-touch, no pointer capture.          |
| Responder props (`onStartShouldSetResponder`, `onResponderGrant`, `onResponderRelease`) | ✅     | React's responder negotiation runs on top of the touch events and is covered by a host test. |
| Accessibility props                                                                     | ❌     | Qt Quick's accessibility bridge is not connected.                                            |

## `Text`

| Aspect                     | Status | Notes                                                                                                     |
| -------------------------- | ------ | --------------------------------------------------------------------------------------------------------- |
| String content             | ✅     | Fragments flattened onto a Qt Quick `Text` item.                                                          |
| `fontSize`, `color`        | ✅     | Applied per fragment.                                                                                     |
| Mixed styling in one block | ✅     | Carried as Qt rich text, because Qt Quick's `Text` applies one font and colour to the whole item.         |
| Wrapping                   | ✅     | Measured with `QTextLayout` against the available width; the item is set to wrap so paint matches layout. |
| `numberOfLines`            | ⚠      | Limits the measured lines; no ellipsis.                                                                   |
| Attachments (inline views) | ❌     | Reported as zero-sized, as React Native's generic manager does.                                           |
| Font family                | ⚠      | Passed to Qt if named; no font registration, so an unavailable family silently falls back.                |

## `Image`

| Aspect                                    | Status | Notes                                                                                                             |
| ----------------------------------------- | ------ | ----------------------------------------------------------------------------------------------------------------- |
| `data:` URIs                              | ✅     | Decoded by `QnxImageProvider` and served as `image://rnqnx/…`, because Qt Quick's `Image` cannot read data URIs.  |
| `file:` and `http(s):`                    | ⚠      | Passed straight to Qt, which fetches them. Untested here — the playground uses a data URI so it needs no network. |
| Bundler assets (`require('./x.png')`)     | ❌     | Needs `resolveAssetSource` and the asset registry.                                                                |
| `defaultSource`, `loadingIndicatorSource` | ❌     | Not carried to the item.                                                                                          |
| `onLoad`, `onError`, `onLoadEnd`          | ❌     | No load lifecycle events reach JavaScript.                                                                        |
| `Image.getSize`, `prefetch`               | ❌     | Static methods are absent from the platform's `Image`.                                                            |
| `resizeMode`                              | ❌     | Qt's `fillMode` is not wired to it.                                                                               |

## Platform modules

| Module                                                  | Status | Notes                                                                   |
| ------------------------------------------------------- | ------ | ----------------------------------------------------------------------- |
| `Platform`                                              | ⚠      | `OS`, `select`, `Version`, and constants from the running system.       |
| `NativeMicrotasksCxx`, `NativeIdleCallbacksCxx`         | ✅     | React Native's own C++ modules, via `DefaultTurboModules`.              |
| `ExceptionsManager`                                     | ✅     | Reports to the host and logs; `dismissRedbox` is an honest no-op.       |
| `DeviceInfo`, `SourceCode`                              | ✅     | Real values: screen metrics from Qt, and the bundle the host evaluated. |
| `FileReaderModule`, `SegmentFetcher`, `WebSocketModule` | ❌     | Referenced by the bundle; unused by the render path.                    |
| `UIManager`                                             | n/a    | Not a TurboModule; bridgeless React Native serves it from JS.           |
