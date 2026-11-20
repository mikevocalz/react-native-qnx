## Props

`StatusBar` is a QNX JavaScript module at `packages/react-native-qnx/Libraries/Components/StatusBar/StatusBar.js`. The component function takes no arguments and returns `null` (`:14-16`), so every prop is ignored. `PublicAPI.js:137` exports it and `tests/js/qnx-public-entry.test.mjs:64` checks the export.

| Prop                                                                                                                        | Type    | Status             | Traced finding                                                                                                                                                                                                  |
| --------------------------------------------------------------------------------------------------------------------------- | ------- | ------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `barStyle`, `hidden`, `backgroundColor`, `translucent`, `networkActivityIndicatorVisible`, `animated`, `showHideTransition` | various | accepted-no-effect | Not read. No native code mentions `StatusBar` (traced: `vnext/src`, `native`, `packages/react-native-qnx`, `packages/expo-router-qnx/src`; the only hits are the module and the router's drawer options below). |

## Events

`StatusBar` has no events and registers no listener.

## Platform behavior on QNX

The module comment says QNX targets typically have no status bar. The static API is present so calls do not throw:

| Static                                                                                                   | Behavior (`StatusBar.js` lines) |
| -------------------------------------------------------------------------------------------------------- | ------------------------------- |
| `setHidden`, `setBarStyle`, `setNetworkActivityIndicatorVisible`, `setBackgroundColor`, `setTranslucent` | Empty function (`:18-22`).      |
| `pushStackEntry`, `replaceStackEntry`                                                                    | Return `0` (`:24-30`).          |
| `popStackEntry`                                                                                          | Empty function (`:27`).         |
| `currentHeight`                                                                                          | `0` (`:32`).                    |

`packages/expo-router-qnx/src/drawerOptions.js:7-14` lists `hideStatusBarOnOpen` and `statusBarAnimation` (the router's `drawerHideStatusBarOnOpen` and `drawerStatusBarAnimation`) as unsupported, with the reason that the window has no status bar, and `docs/component-support.md` records the same. The window title bar that window chrome can draw is separate and sits outside the React surface; see [window chrome](/docs/how-to/window-chrome).

## Minimal example

```tsx
import React from 'react';
import { StatusBar, Text, View } from 'react-native';

export default function Screen() {
  StatusBar.setHidden(true);
  return (
    <View style={{ flex: 1 }}>
      <StatusBar barStyle="light-content" backgroundColor="#000000" />
      <Text>No status bar is drawn on QNX.</Text>
    </View>
  );
}
```

Both the element and the static call compile and run, and neither changes what is on screen. `StatusBar.currentHeight` reads `0`, so layouts that add it as padding get no extra space.

## Known gaps

- No status bar exists on the QNX target, and nothing in the repository implements one.
- No device receipt covers this module.
