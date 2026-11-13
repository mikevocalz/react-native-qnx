/**
 * Linking for QNX.
 *
 * React Native's Linking calls a `LinkingManager` TurboModule that the QNX
 * host does not provide, and its `getInitialURL` then fails on a null module
 * (`nullthrows`). expo-router reads the initial URL when it mounts, so the app
 * would not render at all. This version keeps the public API and answers from
 * what a desktop window has: no launch URL, no URL events, and no handler to
 * open another app's URL. `openURL` and `openSettings` reject; `canOpenURL`
 * resolves false. Opening a URL in the system browser needs a host module and
 * is not implemented.
 */

'use strict';

const EventEmitter = require('react-native/Libraries/vendor/emitter/EventEmitter').default;

const emitter = new EventEmitter();

const Linking = {
  addEventListener(type, listener) {
    return emitter.addListener(type, listener);
  },
  /** Test and host hook: deliver a `url` event as if the OS had opened one. */
  _emitURL(url) {
    emitter.emit('url', { url });
  },
  getInitialURL() {
    return Promise.resolve(null);
  },
  canOpenURL(_url) {
    return Promise.resolve(false);
  },
  openURL(url) {
    return Promise.reject(new Error(`Linking.openURL is not implemented on QNX (${url}).`));
  },
  openSettings() {
    return Promise.reject(new Error('Linking.openSettings is not implemented on QNX.'));
  },
  sendIntent() {
    return Promise.reject(new Error('Linking.sendIntent is Android only.'));
  },
};

export default Linking;
