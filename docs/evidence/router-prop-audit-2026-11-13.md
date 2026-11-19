# expo-router 58.0.31 Tabs and Drawer options on QNX (2026-11-13)

Audit of every option and prop that `expo-router`'s `Tabs` and `Drawer` pass to
the navigators and the views that draw them, and what the QNX implementation
does with each.

## Sources read

`pnpm-lock.yaml` has no `@react-navigation/bottom-tabs` or
`@react-navigation/drawer` entry. expo-router 58.0.31 vendors react-navigation
in `build/react-navigation/` (`bottom-tabs`, `drawer`, `elements`), so that copy
is the source of truth. `react-native-drawer-layout@4.2.12` is in the lock and
is what the vendored `DrawerView` renders. Paths below start at
`apps/component-gallery/node_modules/expo-router/build/`.

| Layer                   | Files                                                                                                                                                            |
| ----------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Tabs wrapper            | `layouts/Tabs.js`, `layouts/TabsClient.js` (`href`, `processScreens`), `standard-navigation/index.js` (`partitionNavigatorProps`)                                |
| Tabs navigator and view | `react-navigation/bottom-tabs/navigators/createBottomTabNavigator.js`, `views/BottomTabView.js`, `views/BottomTabBar.js`, `views/BottomTabItem.js`, `types.d.ts` |
| Drawer wrapper          | `layouts/Drawer.js`, `layouts/DrawerClient.js`                                                                                                                   |
| Drawer navigator, view  | `react-navigation/drawer/navigators/createDrawerNavigator.js`, `views/DrawerView.js`, `DrawerContent.js`, `DrawerItemList.js`, `DrawerItem.js`, `types.d.ts`     |
| Drawer layout           | `react-native-drawer-layout@4.2.12`: `src/types.tsx`, `src/views/Drawer.native.tsx`                                                                              |

The resolver (`packages/expo-router-qnx/src/resolver.js`) replaces
`react-native-drawer-layout` with `drawer-layout.js` (`QtDrawer` or `JsDrawer`),
vendored `BottomTabBar.js` with `BottomTabBar.js` (native Qt bar or `JsTabBar`),
and `bottom-tabs/views/ScreenFallback.js` with `shims/tabs-screen-fallback.js`.
`BottomTabView`, `DrawerView`, the drawer content components and the
`elements` header run unchanged. `useLegacyImplementation` does not exist in
expo-router 58.

Status words: **works** (drawn or run on QNX, test named), **works, JS bar**
(the option sends the tab bar to `JsTabBar`, which draws it), **core** (handled
in navigation JavaScript before any view), **warns** (the platform cannot do
it; one `console.warn` per name under `__DEV__`, entry in
`docs/component-support.md`).

## Tabs navigator props

| Prop                                                                                                                                                                         | Where it is read                                                         | QNX                                                                                                                                                                                      |
| ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `id`, `initialRouteName`, `backBehavior`, `screenOptions`, `screenListeners`, `screenLayout`, `layout`, `UNSTABLE_router`, `activityEnabled`, `unstable_screenErrorBoundary` | `standard-navigation/index.js:138-160`, `createBottomTabNavigator.js:31` | core                                                                                                                                                                                     |
| `tabBar`                                                                                                                                                                     | `BottomTabView.js:116,209`                                               | works: a custom renderer bypasses the replaced bar                                                                                                                                       |
| `safeAreaInsets`                                                                                                                                                             | `BottomTabView.js:116,203`                                               | works: `BottomTabBar.js` pads by `insets.bottom` (0 with the safe-area shim)                                                                                                             |
| `detachInactiveScreens`                                                                                                                                                      | `BottomTabView.js:116,238,273`                                           | works, by another route: no `react-native-screens`, so inactive tabs are `display: none` and `pointerEvents: none` (`shims/tabs-screen-fallback.js`). Yoga skips them; they stay mounted |
| `href` (Tabs screen)                                                                                                                                                         | `TabsClient.js:85-121`                                                   | works: `href: null` hides the tab; a `tabBarButton` built from `href` is now drawn by `JsTabBar`, so the `Link` wrapper runs                                                             |

## Tabs screen options

| Option                                                                                                                                                                                                                 | Where it is read                                                | QNX                                                                                                                                                                                                    | Test                                        |
| ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------- |
| `hidden`, `title`                                                                                                                                                                                                      | `useVisibleTabsWithRedirect.js:39`, `BottomTabBar.js:335`       | works (`tabBarModel.js` `buildTabs`)                                                                                                                                                                   | `qnx-router-tab-options` (labels)           |
| `lazy`, `popToTopOnBlur`, `sceneStyle`, `animation`, `transitionSpec`, `sceneStyleInterpolator`                                                                                                                        | `usePreloadPlaceholderRoutes.js:23`, `BottomTabView.js:133-292` | core / `BottomTabView` unchanged. `animation: 'fade'` and `'shift'` run in `BottomTabView`; during a transition the outgoing tab has state 1, so the fallback keeps it shown                           | none (navigation JS)                        |
| `freezeOnBlur`                                                                                                                                                                                                         | `types.d.ts:224-227` ("deprecated, no effect")                  | nothing to do: no reader exists upstream                                                                                                                                                               |                                             |
| `tabBarPosition`                                                                                                                                                                                                       | `BottomTabView.js:226-299`                                      | works: `qnxTabsScreenOptions` sets it from the window width; edge in `tabBarModel.js` `resolveTabBarEdge`                                                                                              | `qnx-router-rendering`, `NativeUiQuickTest` |
| `tabBarShowLabel`                                                                                                                                                                                                      | `BottomTabBar.js:369`                                           | works, native and JS                                                                                                                                                                                   | `qnx-router-tab-options`                    |
| `tabBarActiveTintColor`, `tabBarInactiveTintColor`                                                                                                                                                                     | `BottomTabBar.js:357-358`                                       | works: native bar takes one pair from the focused tab; per-tab values draw in `JsTabBar`                                                                                                               | `qnx-router-tab-options`                    |
| `tabBarActiveBackgroundColor`, `tabBarInactiveBackgroundColor`                                                                                                                                                         | `BottomTabBar.js:359-360`                                       | works, JS bar                                                                                                                                                                                          | `qnx-router-tab-options`                    |
| `tabBarStyle`                                                                                                                                                                                                          | `BottomTabBar.js:231,301`                                       | works: `backgroundColor`, `height`, `display: 'none'` natively; every other key sends the bar to `JsTabBar`, which applies the whole style                                                             | `qnx-router-tab-options`                    |
| `tabBarBackground`                                                                                                                                                                                                     | `BottomTabBar.js:248,310`                                       | works, JS bar (drawn behind the items)                                                                                                                                                                 | `qnx-router-tab-options`                    |
| `tabBarItemStyle`, `tabBarLabelStyle`, `tabBarIconStyle`, `tabBarAllowFontScaling`                                                                                                                                     | `BottomTabBar.js:356,370-376`                                   | works, JS bar                                                                                                                                                                                          | `qnx-router-tab-options`                    |
| `tabBarIcon`                                                                                                                                                                                                           | `BottomTabBar.js:362`                                           | works, JS bar (called with `focused`, `color`, `size`); `qnxIcon` draws natively                                                                                                                       | `qnx-router-tab-options`                    |
| `tabBarBadge`, `tabBarBadgeStyle`                                                                                                                                                                                      | `BottomTabBar.js:366-367`                                       | works: natively `backgroundColor` and `color`; any other badge style key sends the bar to `JsTabBar`                                                                                                   | `qnx-router-tab-options`                    |
| `tabBarAccessibilityLabel`, `tabBarButtonTestID`                                                                                                                                                                       | `BottomTabBar.js:339,355`                                       | works (`Accessible.name` natively; `accessibilityLabel` and `testID` in JS)                                                                                                                            | `qnx-router-tab-options`                    |
| `tabBarButton`                                                                                                                                                                                                         | `BottomTabItem.js:42,87`                                        | works, JS bar: called with `accessibilityRole`, `role`, `accessibilityState`, `testID`, `onPress`, `onLongPress`, `style`, `children`, `href`                                                          | `qnx-router-tab-options`                    |
| `tabBarVariant`                                                                                                                                                                                                        | `BottomTabBar.js:166-172`                                       | works, JS bar: `material` draws the active indicator pill around the icon                                                                                                                              | `qnx-router-tab-options`                    |
| `tabBarLabelPosition`                                                                                                                                                                                                  | `BottomTabBar.js:98-105,170`                                    | works, JS bar: `beside-icon` lays the item out as a row                                                                                                                                                | `qnx-router-tab-options`                    |
| `tabBarHideOnKeyboard`, `tabBarVisibilityAnimationConfig`                                                                                                                                                              | `BottomTabBar.js:166,175-203`                                   | works: `BottomTabBar.js` follows `Keyboard` `keyboardDidShow` and `keyboardDidHide` and animates opacity with the config. QNX has no on-screen keyboard unless one is installed, so the bar then stays | `qnx-router-tab-options`                    |
| Header options (`header`, `headerShown`, `headerTitle*`, `headerLeft`, `headerRight`, `headerStyle`, `headerTintColor`, `headerBackground`, `headerTransparent`, `headerShadowVisible`, `headerSearchBarOptions`, ...) | `BottomTabView.js:249-253`, `elements/Header/Header.js:141`     | works: `BottomTabView` and `elements` run unchanged. Not checked visually: the safe-area shim reports zero insets                                                                                      | the gallery runs a custom `header`          |

The native bar draws the options in the first rows. `needsJsTabBar`
(`tabBarModel.js`) lists what it cannot draw; any such option on any tab moves
the whole bar to `JsTabBar`. `docs/component-support.md` states this next to
`RNQnxTabBar`.

## Drawer navigator props

| Prop                                                                                                                                                                         | Where it is read                                   | QNX                                                                                                                                     |
| ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------- |
| `id`, `initialRouteName`, `backBehavior`, `screenOptions`, `screenListeners`, `screenLayout`, `layout`, `UNSTABLE_router`, `activityEnabled`, `unstable_screenErrorBoundary` | `standard-navigation/index.js:138-160`             | core                                                                                                                                    |
| `defaultStatus`                                                                                                                                                              | `createDrawerNavigator.js:31`, `DrawerView.js:129` | core: it becomes the `open` prop; the first state is not animated (`drawerStartsOpenWithoutAnimating`)                                  |
| `drawerContent`                                                                                                                                                              | `DrawerView.js:100,207`                            | works: passed as `renderDrawerContent`                                                                                                  |
| `detachInactiveScreens`                                                                                                                                                      | `DrawerView.js:100,219`                            | works, by another route: the screens shim is off, `ScreenFallback` falls back to `ResourceSavingView`; inactive scenes are not laid out |

## Drawer screen options and `react-native-drawer-layout` props

`DrawerView` reads the focused route's options and passes them to the layout
(`DrawerView.js:271-317`). QtDrawer and JsDrawer receive exactly these.

| Option / prop                                               | Passed as                      | QtDrawer                                                                                                                               | JsDrawer                                       | Test                                                                                            |
| ----------------------------------------------------------- | ------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------- | ----------------------------------------------------------------------------------------------- |
| `drawerType`                                                | `drawerType`                   | works: `front`, `slide`, `back` natively; `permanent` is a row                                                                         | works                                          | `NativeUiQuickTest` (front, slide, back), `qnx-router-native-drawer`                            |
| `drawerPosition`                                            | `drawerPosition`               | works: native `side`, right-hand panel by transform                                                                                    | works                                          | `NativeUiQuickTest`, `qnx-router-native-drawer`                                                 |
| `drawerStyle`                                               | `drawerStyle`                  | works: width sets the panel width; the whole style is painted on the panel (this was dropped before)                                   | works                                          | `qnx-router-native-drawer`                                                                      |
| `overlayColor`                                              | `overlayStyle.backgroundColor` | works: native `scrimColor`                                                                                                             | works                                          | `NativeUiQuickTest` `overlayColorIsTheScrimColor`, `qnx-router-native-drawer`                   |
| `overlayAccessibilityLabel`                                 | same                           | works: `accessibilityLabel` becomes the scrim's `scrimLabel` (`QtNativeUiBridge.cpp` `applyDrawer`)                                    | works                                          | `qnx-router-native-drawer`                                                                      |
| `swipeEnabled`                                              | `swipeEnabled`                 | works                                                                                                                                  | works                                          | `NativeUiQuickTest` `swipeDisabledIgnoresDrags`                                                 |
| `swipeEdgeWidth`                                            | `swipeEdgeWidth`               | works: native `edgeWidth`, strip on either side                                                                                        | works                                          | `NativeUiQuickTest` `swipeEdgeWidthSetsTheStartStrip`, `rightDrawerOpensFromTheRightEdgeStrip`  |
| `swipeMinDistance`                                          | `swipeMinDistance`             | works: native `swipeMinDistance`, a drag shorter than it settles back unless it is a fling                                             | works                                          | `NativeUiQuickTest` `swipeMinDistance...`, `qnx-router-native-drawer`                           |
| `keyboardDismissMode`                                       | `keyboardDismissMode`          | works: a drag start calls `Keyboard.dismiss()` unless `'none'`                                                                         | works                                          | `qnx-router-native-drawer`                                                                      |
| `onGestureStart`, `onGestureEnd`, `onGestureCancel`         | same                           | works: new native events `onGestureStart`, `onGestureEnd`, `onGestureCancel` from the drag handlers                                    | works (PanResponder grant, release, terminate) | `NativeUiQuickTest` `gestureEventsBracketADragAndCancelSettlesBack`, `qnx-router-native-drawer` |
| `onOpen`, `onClose`, `onTransitionStart`, `onTransitionEnd` | same                           | works                                                                                                                                  | works                                          | `qnx-router-native-drawer`                                                                      |
| `hideStatusBarOnOpen`                                       | `drawerHideStatusBarOnOpen`    | warns: the QNX window has no status bar; `StatusBar` is a no-op (`docs/component-support.md`)                                          | warns                                          | `qnx-router-native-drawer`                                                                      |
| `statusBarAnimation`                                        | `drawerStatusBarAnimation`     | warns, same reason                                                                                                                     | warns                                          | `qnx-router-native-drawer`                                                                      |
| `configureGestureHandler`                                   | same                           | warns: `react-native-gesture-handler` is not available (`docs/reanimated-gesture-handler-qnx-map.md`)                                  | warns                                          | `qnx-router-native-drawer`                                                                      |
| `direction`                                                 | `direction`                    | not needed: `drawerPosition` already carries the resolved side                                                                         | same                                           |                                                                                                 |
| `swipeMinVelocity`, `drawerWidth`                           | not passed by `DrawerView`     | the drawers accept `drawerWidth`; width otherwise comes from `drawerStyle.width` or `min(320, max(240, 0.8 * layout.width))`           |                                                | `qnx-router-native-drawer`                                                                      |
| `useDrawerProgress`, `DrawerProgressContext`                | context                        | works on both paths: `QtDrawer` provides an `Animated.Value` that follows `open` with the native duration; `JsDrawer` provides its own | works                                          | `qnx-router-native-drawer`                                                                      |
| `DrawerGestureContext`                                      | context                        | exported, never provided (it holds a gesture-handler ref; none exists)                                                                 | same                                           |                                                                                                 |

The panel's items are React Native views. The native drawer draws hover,
pressed and keyboard-focus layers over the accessible button-like items in the
panel, and handles Up, Down, Home, End, Tab, Shift+Tab, Enter, Space and
Escape, with a focus trap while open and focus restored to the previous owner
on close (`NativeUiQuickTest`: `drawerItemsShowHoverAndPressedStates`,
`drawerKeyboardMovesFocusAndPressesItems`, `drawerSkipsDisabledAndHiddenItems`,
`drawerHoldsFocusWhileOpenAndGivesItBack`).

## Default drawer content

`drawerContentContainerStyle`, `drawerContentStyle`, `drawerActiveTintColor`,
`drawerInactiveTintColor`, `drawerActiveBackgroundColor`,
`drawerInactiveBackgroundColor`, `drawerLabel`, `drawerIcon`, `drawerItemStyle`,
`drawerLabelStyle`, `drawerAllowFontScaling` and `title` are read by
`DrawerContent`, `DrawerItemList` and `DrawerItem` (`DrawerItemList.js:71-105`,
`DrawerContent.js:22-28`). Those render as plain React Native views and run
unchanged. Header options on drawer screens go through the unchanged
`elements` header, with the same visual caveat as for tabs.

## Warned options

Three drawer props have no QNX behavior to give, each warned once per name
under `__DEV__` (`packages/expo-router-qnx/src/devWarnings.js`,
`drawerOptions.js`):

- `drawerHideStatusBarOnOpen` (`hideStatusBarOnOpen`) and
  `drawerStatusBarAnimation` (`statusBarAnimation`): there is no status bar
  surface. `StatusBar` is a documented no-op ("The target has no status-bar
  integration", `docs/component-support.md`).
- `configureGestureHandler`: it customizes a `react-native-gesture-handler` pan
  gesture; neither drawer uses that package
  (`docs/reanimated-gesture-handler-qnx-map.md`).

## Not verified

- Header height and insets with the safe-area shim.
- `animation: 'fade'` and `'shift'` visually with the display-none fallback.
- Hover, pressed and focus layers and keyboard navigation run in the Qt host
  tests (offscreen, software renderer) and not on the device. Pressing a focused
  item emits the item's Qt accessibility press action, which needs the host
  built with the accessibility bridge (`RNQNX_ENABLE_ACCESSIBILITY_BRIDGE`).
