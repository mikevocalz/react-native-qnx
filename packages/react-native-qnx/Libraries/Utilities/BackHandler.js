/**
 * BackHandler for QNX.
 *
 * React Native's `BackHandler.js` only re-exports `./BackHandler`, expecting a
 * platform file; without one the import resolves to itself and the export is
 * undefined. QNX desktop and embedded targets have no system back gesture or
 * hardware back key, so, like iOS, listeners are accepted and never called.
 * LogBox registers one, which is how this was found.
 */

'use strict';

function emptyFunction() {}

const BackHandler = {
  exitApp: emptyFunction,
  addEventListener(_eventName, _handler) {
    return { remove: emptyFunction };
  },
};

export default BackHandler;
