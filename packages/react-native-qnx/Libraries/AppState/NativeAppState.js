'use strict';

// React Native 0.88's default NativeAppState spec uses getEnforcing(), which
// assumes the platform always registers AppState. QNX/Marathon keep the native
// lifecycle adapter opt-in while it is being qualified: return the *actual*
// registered module when present and null when disabled. No dummy module,
// synthetic events or second JavaScript lifecycle implementation.
const { get } = require('react-native/Libraries/TurboModule/TurboModuleRegistry');

module.exports = get('AppState');
