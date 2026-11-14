// Metro resolver for expo-router on the qnx platform.
//
// expo-router 58 lays its navigators out with react-navigation code it bundles
// itself, and that code reaches for four packages with native halves QNX does
// not have. For the qnx platform only, this resolver substitutes:
//
//   react-native-safe-area-context  -> shims/safe-area-context.js (zero insets)
//   react-native-screens            -> shims/screens.js (screensEnabled() false)
//   react-native-drawer-layout      -> drawer-layout.js (native Qt drawer or JS)
//   .../bottom-tabs/views/BottomTabBar -> BottomTabBar.js (native Qt tab bar or JS)
//   expo/build/winter/fetch -> shims/expo-fetch.js (no ExpoFetchModule on QNX)
//   expo-linking/build/ExpoLinking -> shims/expo-linking-native.js
//   .../bottom-tabs/views/ScreenFallback -> shims/tabs-screen-fallback.js (inactive tabs are display: none)
//
// `import { Tabs } from 'expo-router'` and `import { Drawer } from
// 'expo-router/drawer'` therefore run unchanged. Other platforms resolve the
// real packages.
const path = require('node:path');

const QNX = 'qnx';

const BY_NAME = new Map([
  ['react-native-safe-area-context', path.join(__dirname, 'shims', 'safe-area-context.js')],
  ['react-native-screens', path.join(__dirname, 'shims', 'screens.js')],
  ['react-native-screens/experimental', path.join(__dirname, 'shims', 'screens-experimental.js')],
  ['react-native-drawer-layout', path.join(__dirname, 'drawer-layout.js')],
]);

// Files, matched after Metro resolves them, that another file replaces.
const BY_FILE = [
  [/[\\/]expo[\\/]build[\\/]winter[\\/]fetch[\\/]index\.js$/, 'expo-fetch.js'],
  [/[\\/]expo-linking[\\/]build[\\/]ExpoLinking\.js$/, 'expo-linking-native.js'],
  [
    /[\\/]expo-router[\\/]build[\\/]react-navigation[\\/]bottom-tabs[\\/]views[\\/]ScreenFallback\.js$/,
    'tabs-screen-fallback.js',
  ],
];
// Deep imports (react-native-screens/lib/commonjs/..., .../jest/mock) reach
// the same native-backed code, so they get the same replacement.
function bySubpath(moduleName) {
  for (const [name, file] of BY_NAME) {
    if (moduleName.startsWith(name + '/')) return file;
  }
  return undefined;
}

const BOTTOM_TAB_BAR =
  /[\\/]expo-router[\\/]build[\\/]react-navigation[\\/]bottom-tabs[\\/]views[\\/]BottomTabBar\.js$/;

/**
 * @param {(context: any, moduleName: string, platform: string | null) => any} next
 *   The resolver to fall back to (the QNX Metro config's own).
 * @param {{ routerContext?: string }} [options] `routerContext` is the app's
 *   file that exports the `require.context` of its `app/` directory; it
 *   replaces `expo-router/_ctx`, which expects Expo's Babel plugin.
 */
function createExpoRouterResolveRequest(next, options = {}) {
  return function resolveRequest(context, moduleName, platform) {
    if (platform === QNX) {
      const shim = BY_NAME.get(moduleName) ?? bySubpath(moduleName);
      if (shim) return { type: 'sourceFile', filePath: shim };
      if (moduleName === 'expo-router/_ctx' && options.routerContext) {
        return { type: 'sourceFile', filePath: options.routerContext };
      }
    }
    const resolution = next
      ? next(context, moduleName, platform)
      : context.resolveRequest(context, moduleName, platform);
    if (platform === QNX && resolution?.type === 'sourceFile') {
      for (const [pattern, shim] of BY_FILE) {
        if (pattern.test(resolution.filePath)) {
          return { type: 'sourceFile', filePath: path.join(__dirname, 'shims', shim) };
        }
      }
    }
    if (
      platform === QNX &&
      resolution?.type === 'sourceFile' &&
      BOTTOM_TAB_BAR.test(resolution.filePath)
    ) {
      return { type: 'sourceFile', filePath: path.join(__dirname, 'BottomTabBar.js') };
    }
    return resolution;
  };
}

module.exports = { createExpoRouterResolveRequest };
