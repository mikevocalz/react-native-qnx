'use strict';
const React = require('react');
const { createStore } = require('zustand/vanilla');
const { useStore } = require('zustand');
const ZERO_INSETS = Object.freeze({ top: 0, right: 0, bottom: 0, left: 0 });
function metrics(snapshot) {
  return {
    frame: { x: 0, y: 0, width: snapshot.contentWidth, height: snapshot.contentHeight },
    insets: snapshot.insets,
  };
}
function readNativeMetrics() {
  const host = globalThis.__rnqnxWindowHost;
  if (!host) return undefined;
  const window = host.getCurrentWindow();
  try {
    return metrics(window.snapshot);
  } finally {
    window.dispose();
  }
}
function useWindowMetrics(fallback) {
  const storeRef = React.useRef(null);
  if (!storeRef.current) storeRef.current = createStore(() => readNativeMetrics() ?? fallback);
  const resourceRef = React.useRef(null);
  React.useEffect(() => {
    const host = globalThis.__rnqnxWindowHost;
    if (!host) return undefined;
    const window = host.getCurrentWindow();
    let subscription;
    const cleanup = () => {
      try {
        subscription?.remove();
      } finally {
        window.dispose();
        resourceRef.current = null;
      }
    };
    try {
      subscription = window.addSnapshotListener((snapshot) =>
        storeRef.current.setState(metrics(snapshot), true)
      );
      resourceRef.current = { window, subscription };
      // Close a render-to-subscription gap without an extra callback or polling.
      storeRef.current.setState(metrics(window.snapshot), true);
    } catch (error) {
      cleanup();
      throw error;
    }
    return cleanup;
  }, []);
  return useStore(storeRef.current);
}
function insetStyle(style, insets, edges = ['top', 'right', 'bottom', 'left'], mode = 'padding') {
  const { StyleSheet } = require('react-native');
  const base = StyleSheet.flatten(style) ?? {};
  const prefix = mode === 'margin' ? 'margin' : 'padding';
  const result = {};
  for (const edge of ['top', 'right', 'bottom', 'left']) {
    const behavior = Array.isArray(edges)
      ? edges.includes(edge)
        ? 'additive'
        : 'off'
      : (edges[edge] ?? 'off');
    if (behavior === 'off') continue;
    const key = prefix + edge[0].toUpperCase() + edge.slice(1);
    const axis = prefix + (edge === 'top' || edge === 'bottom' ? 'Vertical' : 'Horizontal');
    const existing = base[key] ?? base[axis] ?? base[prefix] ?? 0;
    if (insets[edge] === 0) continue;
    if (typeof existing !== 'number')
      throw new Error('QNX SafeAreaView requires numeric inset-edge padding or margin');
    result[key] =
      behavior === 'maximum' ? Math.max(existing, insets[edge]) : existing + insets[edge];
  }
  return result;
}
module.exports = { useWindowMetrics, readNativeMetrics, insetStyle, ZERO_INSETS };
