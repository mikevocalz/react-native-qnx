'use strict';

// Nitro v1 places the public API and the global proxy contract in nitro-core.
// Importing react-native-nitro-modules would also load its Android/iOS Turbo
// module path, which is not a valid QNX install route.
const { NitroModules } = require('nitro-core');

function createQnxPlatformInfo() {
  const QnxPlatformInfo = NitroModules.getHybridClass('QnxPlatformInfo');
  return new QnxPlatformInfo();
}

// ---------------------------------------------------------------------------
// Windows API (docs/nitro/API_CONTRACTS.md, "Windows API (S8)"). The native
// QnxWindows class is a flat adapter: numeric listener ids, validated strings,
// callbacks that return nothing. This file builds the public contract on it.
// ---------------------------------------------------------------------------

/** Native rejections and throws carry `CODE: reason`; the code becomes error.code. */
function mapWindowsError(error) {
  const message = error instanceof Error ? error.message : String(error);
  const match = /^([A-Z][A-Z_]+): ([\s\S]*)$/.exec(message);
  if (match === null) return error instanceof Error ? error : new Error(message);
  const mapped = new Error(match[2]);
  mapped.code = match[1];
  return mapped;
}

function callNative(fn) {
  try {
    return fn();
  } catch (error) {
    throw mapWindowsError(error);
  }
}

function callNativeAsync(fn) {
  let result;
  try {
    result = fn();
  } catch (error) {
    return Promise.reject(mapWindowsError(error));
  }
  return Promise.resolve(result).catch((error) => {
    throw mapWindowsError(error);
  });
}

function reportGuardError(error) {
  const reporter = globalThis.reportError;
  if (typeof reporter === 'function') reporter(error);
  else console.error(error);
}

/** One idempotent remove closure for a native listener id. */
function unsubscriber(native, id) {
  let removed = false;
  return () => {
    if (removed) return;
    removed = true;
    try {
      native.removeListener(id);
    } catch {
      // Torn down: nothing left to remove.
    }
  };
}

function toWindowState(data) {
  return {
    sequence: data.sequence,
    title: data.title,
    size: { width: data.width, height: data.height },
    position: { x: data.x, y: data.y },
    focused: data.focused,
    visible: data.visible,
    maximized: data.maximized,
    controlsLayout: data.controlsLayout,
  };
}

function toInset(top, right, bottom, left, overlayX, overlayY, overlayWidth, overlayHeight) {
  return {
    top,
    right,
    bottom,
    left,
    overlay: { x: overlayX, y: overlayY, width: overlayWidth, height: overlayHeight },
  };
}

function createWindowsApi(native) {
  const capabilities = Object.freeze(Array.from(native.capabilities));
  return Object.freeze({
    capabilities,
    getState: () => callNative(() => toWindowState(native.getState())),
    setControlsLayout: (layout) => callNativeAsync(() => native.setControlsLayout(layout)),
    setTitle: (title) => callNativeAsync(() => native.setTitle(String(title))),
    setBounds: (bounds) =>
      callNativeAsync(() =>
        native.setBounds({
          x: bounds.x,
          y: bounds.y,
          width: bounds.width,
          height: bounds.height,
        })
      ),
    minimize: () => callNativeAsync(() => native.minimize()),
    close: (options) => callNativeAsync(() => native.close(options?.force === true)),
    addCloseGuard: (guard) => {
      if (typeof guard !== 'function') {
        const error = new Error('guard must be a function');
        error.code = 'INVALID_ARGUMENT';
        throw error;
      }
      const id = callNative(() =>
        native.addCloseGuard((requestId, guardId, reason, cancellable, deadlineMs) => {
          const request = { reason, cancellable };
          if (deadlineMs >= 0) request.deadlineMs = deadlineMs;
          // A synchronous guard is accepted and becomes a Promise here.
          let outcome;
          try {
            outcome = Promise.resolve(guard(request));
          } catch (error) {
            outcome = Promise.reject(error);
          }
          outcome.then(
            // Only `true` lets the window close.
            (value) => answer(requestId, guardId, value === true),
            (error) => {
              // A guard that throws or rejects cancels the close and reports once.
              reportGuardError(error);
              answer(requestId, guardId, false);
            }
          );
        })
      );
      return unsubscriber(native, id);
    },
    getTitleBarInset: () =>
      callNative(() => {
        const i = native.getTitleBarInset();
        return toInset(
          i.top,
          i.right,
          i.bottom,
          i.left,
          i.overlayX,
          i.overlayY,
          i.overlayWidth,
          i.overlayHeight
        );
      }),
    addOnInsetChangedListener: (listener) =>
      unsubscriber(
        native,
        callNative(() =>
          native.addOnInsetChangedListener((...values) => listener(toInset(...values)))
        )
      ),
    addOnFocusChangedListener: (listener) =>
      unsubscriber(
        native,
        callNative(() => native.addOnFocusChangedListener(listener))
      ),
    addOnVisibilityChangedListener: (listener) =>
      unsubscriber(
        native,
        callNative(() => native.addOnVisibilityChangedListener(listener))
      ),
  });

  function answer(requestId, guardId, allow) {
    try {
      native.resolveCloseGuard(requestId, guardId, allow);
    } catch {
      // The runtime was torn down while the guard ran; the close proceeds.
    }
  }
}

let windows;

function getQnxWindows() {
  if (windows !== undefined) return windows;
  let native;
  try {
    const QnxWindows = NitroModules.getHybridClass('QnxWindows');
    native = new QnxWindows();
  } catch (error) {
    const mapped = mapWindowsError(error);
    if (mapped.code !== undefined) throw mapped;
    const unavailable = new Error(
      'The QNX windows API is not installed in this host: ' + mapped.message
    );
    unavailable.code = 'WINDOWS_UNAVAILABLE';
    throw unavailable;
  }
  windows = createWindowsApi(native);
  return windows;
}

module.exports = { createQnxPlatformInfo, getQnxWindows };
