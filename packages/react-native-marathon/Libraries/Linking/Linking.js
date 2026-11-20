/**
 * `Linking` for Marathon.
 *
 * React Native's Linking needs a `LinkingManager` TurboModule that the Marathon
 * host does not provide, and expo-router reads the initial URL at mount. This
 * version keeps the public API and answers from what the host provides today:
 * no launch URL, no URL events, no way to open another app. `openURL` and
 * `openSettings` reject with a Marathon-specific error and `canOpenURL`
 * resolves false. The shell has a D-Bus route for `marathon://` URLs only and no http
 * handler of its own, so there is no general contract to bridge; see
 * docs/marathon/SHELL_CONTRACTS.md (Marathon-Shell source read 2026-11-20).
 */
'use strict';

const EventEmitter = require('react-native/Libraries/vendor/emitter/EventEmitter').default;

const emitter = new EventEmitter();

const Linking = {
  addEventListener(type, listener) {
    return emitter.addListener(type, listener);
  },
  /** Test and host hook: deliver a `url` event as if the shell had opened one. */
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
    return Promise.reject(new Error(`Linking.openURL is not available on Marathon (${url}).`));
  },
  openSettings() {
    return Promise.reject(new Error('Linking.openSettings is not available on Marathon.'));
  },
  sendIntent() {
    return Promise.reject(new Error('Linking.sendIntent is Android only.'));
  },
};

module.exports = Linking;
module.exports.default = Linking;
