# Opt in to the partial public React Native entry

This Phase3 prerequisite lets supported application code use named imports from react-native instead of importing its internal Libraries paths. It reuses the existing Qt/Fabric component implementations. It is explicitly opt-in and does not change the default QNX or Marathon resolver.

Wrap your existing target Metro config with the public subpath helper:

```js
const withPublicEntry = require('@rn-qnx/metro-config-qnx/public-entry');
const { getQnxMetroConfig } = require('@rn-qnx/metro-config-qnx');
module.exports = withPublicEntry(getQnxMetroConfig(__dirname), 'qnx');
```

For Marathon, wrap the existing getMarathonMetroConfig result and select marathon. Other platform requests and non-root requests retain the original resolver. This subpath is included in the package file list, but no new package publication is claimed.

## Available imports and their limits

The entry exposes 30 lazy imports. Each resolves to the existing React Native implementation or the platform's Qt/Fabric override. Composite controls reuse the same Qt primitives. See the [component support inventory](../component-support.md) for each component's rendering and interaction limits.

| Group                 | Public exports                                                                                          | Boundary                                                                                                                                                                                                                             |
| --------------------- | ------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Runtime               | `AppRegistry`, `AppState`, `Platform`                                                                   | AppState availability still depends on its optional registered native module.                                                                                                                                                        |
| Core views            | `View`, `Text`, `Image`, `ScrollView`, `TextInput`, `Pressable`, `ActivityIndicator`, `Switch`, `Modal` | Existing Qt/Fabric implementation and component-specific limits apply.                                                                                                                                                               |
| Composites            | `Button`, `TouchableOpacity`, `ImageBackground`, `FlatList`, `VirtualizedList`, `SectionList`           | Existing React components compose the same platform primitives; these exports do not add separate Qt widgets.                                                                                                                        |
| Style and geometry    | `StyleSheet`, `processColor`, `Dimensions`, `useWindowDimensions`, `PixelRatio`                         | Dimensions and scale come from the existing DeviceInfo provider. On this main baseline it reports screen metrics for both screen and window; live window resize is a separate native capability.                                     |
| Motion and responders | `Animated`, `Easing`, `PanResponder`                                                                    | Uses React Native Animated and its responder system. Device gesture delivery and the selected Animated driver's existing restrictions still apply.                                                                                   |
| Renderer reference    | `findNodeHandle`                                                                                        | The upstream renderer function; native reference lifetime and component support remain unchanged.                                                                                                                                    |
| Layout direction      | `I18nManager`                                                                                           | The unmodified upstream implementation uses its fixed-LTR fallback when no native I18nManager is registered. `isRTL` is false; native RTL configuration methods cannot change direction. Native RTL is unsupported on this baseline. |

`AccessibilityInfo` and `BackHandler` are also public imports. Their native operations require the opt-in Qt platform-services provider; unsupported preferences fail explicitly. See the [native contract](../native-modules/qt-accessibility-back.md).

Unsupported public imports, including `Alert`, `Appearance`, `Keyboard`, `Linking` and `useColorScheme`, still throw an explicit error. Code that imports them needs an implementation before its feature can run. CommonJS interop metadata and promise introspection are exempt from those errors. `RefreshControl`, `KeyboardAvoidingView` and `StatusBar` are not added to this public entry merely because no-op internal overrides exist.

Keyboard, pointer hover and imperative focus for generic Views are separately enabled with `RNQNX_ENABLE_DESKTOP_INPUT=ON` and `RNQNX_ENABLE_INPUT_LIFECYCLE=ON`. Both default OFF. The [desktop input contract](../native-modules/qt-desktop-input.md) explains event delivery, native focus traversal and the selected shared-control validation boundary.

This entry does not add Expo Router, native screen stacks, Worklets, Reanimated, Gesture Handler or SVG interoperability. Their native registrations and runtime contracts need separate validation. No support rows or release gates are promoted by adding imports.

## Installed packages with Qt entries

`getQnxMetroConfig` asserts the `qnx` package export condition only for the QNX target. `getMarathonMetroConfig` similarly asserts `marathon`. Both preserve React Native's conditions, other platforms' conditions and custom project conditions. This also works without opting in to the partial public entry.

A package with distinct Qt and mobile providers can declare explicit entries:

```json
{
  "exports": {
    ".": {
      "qnx": "./dist/qt/index.js",
      "marathon": "./dist/qt/index.js",
      "react-native": "./dist/native/index.js",
      "default": "./dist/web/index.js"
    },
    "./slider": {
      "qnx": "./dist/qt/slider.js",
      "marathon": "./dist/qt/slider.js",
      "react-native": "./dist/native/slider.js",
      "default": "./dist/web/slider.js"
    }
  }
}
```

Put the target keys before `react-native` and `default`: the package's key order controls which matching condition wins. An explicit `./dist/native/slider.js` export otherwise bypasses platform suffix selection. A `Slider.qnx.js` sibling does not replace that explicit native entry.

The Qt entry must itself avoid loading unavailable mobile providers, including imports made at module scope. A package with only a `react-native` entry still resolves to that entry. These conditions do not infer compatibility, install native modules or convert SwiftUI/Android views into Qt controls. Selected premium packages remain optional app dependencies; none is needed by these factories or core tests.

## Validation

The expanded `public-api-probe.js` imports the newly exposed composites and utilities, checks helper semantics and DeviceInfo consistency when executed, and renders through the existing `RNQnxPlayground` surface. Both target bundles build with real Metro. The package-condition regression separately builds and executes a small installed fixture for QNX, Marathon, iOS, Android and web, verifying root/subpath selection, React Native fallback and isolation from the mobile provider.

The [dated validation receipt](../validation/qt-public-component-equivalence-2026-11-10.md) separates those JavaScript checks from full Qt/Hermes rendering and device qualification. A bundle or Node VM check does not prove native input, rendering, QNX hardware or Marathon shell behavior. Existing native evidence remains bounded by the component inventory.
