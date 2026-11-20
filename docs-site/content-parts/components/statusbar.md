## Props

`StatusBar` is a QNX-specific JavaScript module at `packages/react-native-qnx/Libraries/Components/StatusBar/StatusBar.js`. The component function returns `null`. It exports the static API of React Native's `StatusBar` as empty functions so calls do not throw.

| Prop                              | Type    | Status             | Source-backed note                |
| --------------------------------- | ------- | ------------------ | --------------------------------- |
| `barStyle`                        | string  | accepted-no-effect | The component takes no arguments. |
| `hidden`                          | boolean | accepted-no-effect | Same.                             |
| `backgroundColor`                 | color   | accepted-no-effect | Same.                             |
| `translucent`                     | boolean | accepted-no-effect | Same.                             |
| `networkActivityIndicatorVisible` | boolean | accepted-no-effect | Same.                             |
| `animated`, `showHideTransition`  | various | accepted-no-effect | Same.                             |

## Events

`StatusBar` has no events.

| Event | Status         | Note                              |
| ----- | -------------- | --------------------------------- |
| none  | not applicable | The module registers no listener. |

## Platform behavior on QNX

The module comment states that QNX targets typically have no status bar to control. The static members are:

| Static                                                                                                   | Behavior        |
| -------------------------------------------------------------------------------------------------------- | --------------- |
| `setHidden`, `setBarStyle`, `setNetworkActivityIndicatorVisible`, `setBackgroundColor`, `setTranslucent` | Empty function. |
| `pushStackEntry`, `replaceStackEntry`                                                                    | Return `0`.     |
| `popStackEntry`                                                                                          | Empty function. |
| `currentHeight`                                                                                          | `0`.            |

`docs/component-support.md` also records that the router's `drawerHideStatusBarOnOpen` and `drawerStatusBarAnimation` have no effect because the window has no status bar.

## Unsupported or partial props

Every prop and static call is ignored. Reason: no status-bar integration exists on the target (`packages/react-native-qnx/Libraries/Components/StatusBar/StatusBar.js`). The window title bar that the window-chrome feature can draw is a separate thing; see [window chrome](/docs/how-to/window-chrome).

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

- No status bar exists on the QNX target. Nothing in the repository implements one.
- No device receipt covers this module; host and device columns are above in the status table.
