# @rn-qnx/expo-router-qnx

Runs [expo-router](https://docs.expo.dev/router/introduction/) 58.0.31 on React
Native QNX with its own `Tabs` and `Drawer`, unchanged in the app:

```js
import { Tabs } from 'expo-router';
import { Drawer } from 'expo-router/drawer';
```

expo-router 58 bundles its own react-navigation. That code reaches for four
packages whose native halves QNX does not have, so a Metro resolver replaces
them for the `qnx` platform only:

| Import                              | Replaced by                                                                           |
| ----------------------------------- | ------------------------------------------------------------------------------------- |
| `react-native-safe-area-context`    | `src/shims/safe-area-context.js`: zero insets, window size from `useWindowDimensions` |
| `react-native-screens`              | `src/shims/screens.js`: `screensEnabled()` is false, navigators use plain Views       |
| `react-native-drawer-layout`        | `src/drawer-layout.js`: native `RNQnxDrawer`, or `JsDrawer` without it                |
| expo-router's `BottomTabBar` module | `src/BottomTabBar.js`: native `RNQnxTabBar`, or `JsTabBar` without it                 |

It also points `expo-router/_ctx` at the app's own `app-ctx.js` (Expo's Babel
plugin normally generates that file's `require.context`), and swaps two Expo
native-module wrappers, `expo/fetch` and `ExpoLinking`, for JavaScript.

## Use

`metro.config.js`:

```js
const { withExpoRouterQnx } = require('@rn-qnx/expo-router-qnx/metro');
module.exports = withExpoRouterQnx(getQnxMetroConfig(__dirname, { ... }), {
  routerContext: path.join(__dirname, 'app-ctx.js'),
});
```

`app-ctx.js` exports `ctx = require.context('./app', true, <route pattern>, 'sync')`;
`index.js` registers `App` from `expo-router/build/qualified-entry` under the host's
component name. A follow-up change makes `rnqnx project create` write all three; today copy them from `apps/component-gallery`.

The Tabs layout turns the window width into a bar position:

```js
import { qnxTabsScreenOptions } from '@rn-qnx/expo-router-qnx';
<Tabs screenOptions={qnxTabsScreenOptions({ width, layout: 'auto', breakpoint: 700 })} />;
```

`layout` is `auto` (bottom bar under `breakpoint`, left rail from it up), `bottom` or
`side`. Give each `Tabs.Screen` `options.qnxIcon` (a glyph or an image URI);
`tabBarIcon` renders React elements and cannot reach the native bar.

## Limits

- No stack navigator is provided or tried. expo-router's default `Stack` is a native stack that
  needs react-native-screens, and its JavaScript stack needs react-native-gesture-handler, which
  is not ported (see `docs/reanimated-gesture-handler-qnx-audit.md`). Native tabs and
  `expo-router/ui` are not supported.
- Inactive tab and drawer screens stay mounted.
- The drawer's edge swipe needs a pointer. The JavaScript drawer also opens from the header
  button, `DrawerActions.toggleDrawer()` and the developer menu entries the gallery adds.
- `Linking.openURL` rejects, and there is no deep-link input.
