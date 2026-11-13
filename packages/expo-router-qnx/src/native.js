// Native component lookup. RNQnxDrawer and RNQnxTabBar are Fabric components
// registered by the QNX host when it is built with RNQNX_ENABLE_QT_ROUTER_UI.
// A host built without them (or an older host) reports false here and the
// callers draw the same UI from plain React Native views.
const UIManager = require('react-native/Libraries/ReactNative/UIManager').default;
const NativeComponentRegistry = require('react-native/Libraries/NativeComponent/NativeComponentRegistry');

function hasNative(name) {
  try {
    return UIManager.hasViewManagerConfig(name);
  } catch {
    return false;
  }
}

const cache = new Map();

/**
 * The Fabric component `name`, with the static view config the JavaScript side
 * declares: `validAttributes` names every prop that crosses to native, and
 * `directEventTypes` maps native event names to handler props.
 */
function nativeComponent(name, { validAttributes, directEventTypes }) {
  if (!cache.has(name)) {
    cache.set(
      name,
      NativeComponentRegistry.get(name, () => ({
        uiViewClassName: name,
        directEventTypes,
        validAttributes,
      }))
    );
  }
  return cache.get(name);
}

module.exports = { hasNative, nativeComponent };
