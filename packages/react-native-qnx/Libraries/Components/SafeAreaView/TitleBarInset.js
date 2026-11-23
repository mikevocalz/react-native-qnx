'use strict';

// The title bar inset the host draws over or above the React surface
// (docs/adr/0016). The host evaluates `globalThis.__RNQNX_TITLE_BAR_INSET__`
// before the surface starts and again whenever the chrome layout changes, then
// emits `rnqnxTitleBarInsetDidChange` on RCTDeviceEventEmitter. With no host
// value (no chrome, or a host without it) every edge is zero.

const GLOBAL_NAME = '__RNQNX_TITLE_BAR_INSET__';
const CHANGE_EVENT = 'rnqnxTitleBarInsetDidChange';
const ZERO = Object.freeze({ top: 0, right: 0, bottom: 0, left: 0 });

function edge(value) {
  return typeof value === 'number' && Number.isFinite(value) && value > 0 ? value : 0;
}

/** Reads the inset the host published; invalid or missing values read as zero. */
function getTitleBarInset(scope = globalThis) {
  const raw = scope[GLOBAL_NAME];
  if (raw == null || typeof raw !== 'object') return ZERO;
  return Object.freeze({
    top: edge(raw.top),
    right: edge(raw.right),
    bottom: edge(raw.bottom),
    left: edge(raw.left),
  });
}

/** Style padding that keeps content clear of the title bar. */
function titleBarInsetPadding(inset) {
  return {
    paddingTop: inset.top,
    paddingRight: inset.right,
    paddingBottom: inset.bottom,
    paddingLeft: inset.left,
  };
}

module.exports = { CHANGE_EVENT, GLOBAL_NAME, getTitleBarInset, titleBarInsetPadding };
