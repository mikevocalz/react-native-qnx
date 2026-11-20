/**
 * `BackHandler` for Marathon.
 *
 * React Native's `BackHandler.js` re-exports `./BackHandler` and expects a
 * platform file.
 *
 * Back. Marathon-Shell sends Back to a native Wayland window as the XF86Back
 * key (docs/NATIVE_APP_LIFECYCLE.md in the shell repository). The Marathon Qt
 * host turns that key into the `hardwareBackPress` device event, the same event
 * React Native's Android BackHandler listens to. Listeners run newest first and
 * stop at the first one that returns true. When none returns true nothing more
 * happens here: the shell sees that the window did not change within
 * `MARATHON_NATIVE_BACK_MS` and backs out of the app itself. A client cannot
 * report "unhandled" in any other way.
 *
 * Exit. The shell closes a window with `xdg_toplevel.close` and treats a client
 * that destroys its window or disconnects as closed. The host exposes that as
 * `globalThis.__rnqnxMarathonExitApp`, which quits the Qt application. A host
 * without it gets one development warning and no exit.
 */
'use strict';

const RCTDeviceEventEmitter =
  require('react-native/Libraries/EventEmitter/RCTDeviceEventEmitter').default;

/** @type {Array<(event?: unknown) => boolean | null | undefined | void>} */
const handlers = [];
let warnedExit = false;

const BackHandler = {
  exitApp() {
    /** @type {{ __rnqnxMarathonExitApp?: () => void, __DEV__?: boolean }} */
    const host = globalThis;
    if (typeof host.__rnqnxMarathonExitApp === 'function') {
      host.__rnqnxMarathonExitApp();
      return;
    }
    if (host.__DEV__ === true && !warnedExit) {
      warnedExit = true;
      console.warn('BackHandler.exitApp has no effect: the host does not expose __rnqnxMarathonExitApp.');
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
  /** Runs the listeners; returns true when one handled the back press. */
  _emitBackPress() {
    for (let index = handlers.length - 1; index >= 0; index -= 1) {
      if (handlers[index]() === true) {
        return true;
      }
    }
    return false;
  },
};

// The host emits this when Marathon-Shell's XF86Back reaches the window.
RCTDeviceEventEmitter.addListener('hardwareBackPress', () => {
  BackHandler._emitBackPress();
});

module.exports = BackHandler;
module.exports.default = BackHandler;
