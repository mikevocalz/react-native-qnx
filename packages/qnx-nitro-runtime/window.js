'use strict';

function getCurrentWindow() {
  const host = globalThis.__rnqnxWindowHost;
  if (!host || typeof host.getCurrentWindow !== 'function') {
    throw new Error('Window API requires a host built with RNQNX_ENABLE_WINDOW_API=ON');
  }
  return host.getCurrentWindow();
}
module.exports = { getCurrentWindow };
