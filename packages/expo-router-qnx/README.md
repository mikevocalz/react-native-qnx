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
The source CLI's `rnqnx project create` already writes the Router context,
entry and Metro configuration for QNX-only projects, using `template/qnx-router`.
Projects that also enable Marathon retain their separately documented starter.
See [project creation](../../docs/getting-started/expo-project-create.md).

The generated starter and [small Router example](../../apps/expo-router-example/README.md)
register `ExpoRoot` from the public `expo-router` export under their configured
component name. Registration stays in `index.js`; the root component and route
context live in separate modules so refresh does not repeat registration.

Wrap `ExpoRoot` with the adapter's `QnxRouterProvider`:

```jsx
import { ExpoRoot } from 'expo-router';
import { QnxRouterProvider } from '@rn-qnx/expo-router-qnx';
import { ctx } from '../app-ctx';

export default function App() {
  return (
    <QnxRouterProvider>
      <ExpoRoot context={ctx} />
    </QnxRouterProvider>
  );
}
```

The provider preserves SDK 58.0.31's Head provider through the public
`expo-router/head` export. Its version-specific layout-refresh import stays
inside the platform adapter; application source uses public exports. The helper loads lazily so importing
other adapter functions in Node tooling does not initialize Router app globals.
This is the repository's QNX adapter, not upstream Expo QNX support or proof of
Fast Refresh delivery to a device. The example uses per-app Zustand state.

The Tabs layout turns the window width into a bar position:

```js
import { qnxTabsScreenOptions } from '@rn-qnx/expo-router-qnx';
<Tabs screenOptions={qnxTabsScreenOptions({ width, layout: 'auto', breakpoint: 700 })} />;
```

`layout` is `auto` (bottom bar under `breakpoint`, left rail from it up), `bottom` or
`side`. Give each `Tabs.Screen` `options.qnxIcon` (a glyph or an image URI);
`tabBarIcon` renders React elements and cannot reach the native bar.

## Props

`Tabs`: `tabBarPosition` (`bottom`, `top`, `left`, `right`; `qnxTabsScreenOptions` sets it
from the window width), `tabBarShowLabel`, `tabBarActiveTintColor`, `tabBarInactiveTintColor`,
`tabBarStyle` (`backgroundColor`, `height`, `display: 'none'`), `tabBarBadge`,
`tabBarBadgeStyle` (`backgroundColor`, `color`), `tabBarLabel`, `tabBarAccessibilityLabel`,
`headerShown` (drawn by react-navigation, above the bar), `hidden` and `tabBarIcon`. A
`tabBarIcon` function is drawn by the JavaScript tab bar for that screen set, because React
elements cannot cross to the native bar; `qnxIcon` keeps the native bar.
Inactive tabs are `display: none`, so they take no layout and no touches.

`Drawer`: `drawerPosition`, `drawerType` (`front`, `slide`, `back`, `permanent`),
`drawerStyle.width`, `overlayStyle.backgroundColor`, `overlayAccessibilityLabel`,
`swipeEnabled` and `swipeEdgeWidth`. Right-to-left follows `I18nManager.isRTL`.

## Limits

- No stack navigator is provided or tried. expo-router's default `Stack` is a native stack that
  needs react-native-screens. Its JavaScript stack needs react-native-gesture-handler, which is
  ported (3.4.0, see `docs/reanimated-gesture-handler-qnx.md`), but a stack on top of it has not
  been built or run here. Native tabs and `expo-router/ui` are not supported.
- Inactive tab and drawer screens stay mounted.
- The drawer's edge swipe needs a pointer. The JavaScript drawer also opens from the header
  button, `DrawerActions.toggleDrawer()` and the developer menu entries the gallery adds.
- `Linking.openURL` rejects, and there is no deep-link input.
