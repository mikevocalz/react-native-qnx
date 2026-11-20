/**
 * `BackHandler` for Marathon.
 *
 * React Native's `BackHandler.js` re-exports `./BackHandler` and expects a
 * platform file. Marathon-Shell owns navigation gestures, and the host has no
 * channel that delivers a back event to JavaScript yet
 * (https://github.com/patrickjquinn/Marathon-Shell). Listeners are kept in
 * registration order and run newest first, as on Android, when a host calls
 * `_emitBackPress()`; that function returns true if a listener handled the
 * event. `exitApp` cannot quit the process from JavaScript and logs one
 * development warning.
 */
'use strict';

/** @type {Array<(event?: unknown) => boolean | null | undefined | void>} */
const handlers = [];
let warnedExit = false;

const BackHandler = {
  exitApp() {
    if (globalThis.__DEV__ === true && !warnedExit) {
      warnedExit = true;
      console.warn('BackHandler.exitApp has no effect on Marathon: the host does not expose exit.');
    }
  },
  addEventListener(_eventName, handler) {
    handlers.push(handler);
    return {
      remove() {
        const index = handlers.indexOf(handler);
        if (index !== -1) {
          handlers.splice(index, 1);
        }
      },
    };
  },
  /** Host hook: returns true when a listener handled the back press. */
  _emitBackPress() {
    for (let index = handlers.length - 1; index >= 0; index -= 1) {
      if (handlers[index]() === true) {
        return true;
      }
    }
    return false;
  },
};

module.exports = BackHandler;
module.exports.default = BackHandler;
