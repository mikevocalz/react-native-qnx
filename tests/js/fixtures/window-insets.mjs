import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
const React = require('react');
const renderer = require(['react', 'test', 'renderer'].join('-'));
const Module = /** @type {any} */ (require('node:module'));
const globals = /** @type {any} */ (globalThis);
globals.IS_REACT_ACT_ENVIRONMENT = true;
const original = Module._load;
const path = require('node:path');
/** @param {string} file @returns {any} */
const load = (file) => require(path.resolve(file));
/** @param {any} style @returns {Record<string, any>} */
const flatten = (style) =>
  Array.isArray(style) ? Object.assign({}, ...style.map(flatten)) : (style ?? {});
/** @type {any} */
const native = {
  View: 'View',
  StyleSheet: { flatten },
  useWindowDimensions: () => ({ width: 700, height: 500 }),
};
/** @param {string} request @param {any} parent @param {boolean} isMain */
Module._load = function (request, parent, isMain) {
  if (request === 'react-native') return native;
  return original.call(this, request, parent, isMain);
};
try {
  const factory = load('packages/qnx-nitro-runtime/window.js');
  assert.throws(() => factory.getCurrentWindow(), /RNQNX_ENABLE_WINDOW_API/);
  let snapshot = {
    contentWidth: 800,
    contentHeight: 600,
    insets: { top: 44, right: 0, bottom: 0, left: 0 },
  };
  /** @type {Set<any>} */
  const handles = new Set();
  globals.__rnqnxWindowHost = {
    getCurrentWindow() {
      /** @type {any} */
      const handle = {
        active: true,
        listener: null,
        get snapshot() {
          return snapshot;
        },
        /** @param {any} listener */
        addSnapshotListener(listener) {
          this.listener = listener;
          return {
            remove: () => {
              this.listener = null;
            },
          };
        },
        dispose() {
          this.active = false;
          this.listener = null;
        },
      };
      handles.add(handle);
      return handle;
    },
  };
  native.__unstableQnxWindowMetrics = load('packages/react-native-qnx/src/window-metrics.js');
  const safe = /** @type {any} */ (load('packages/expo-router-qnx/src/shims/safe-area-context.js'));
  /** @type {any} */
  let observed = {};
  function Probe() {
    observed = { insets: safe.useSafeAreaInsets(), frame: safe.useSafeAreaFrame() };
    return React.createElement(
      safe.SafeAreaView,
      { testID: 'safe', style: { paddingTop: 4 } },
      'content'
    );
  }
  /** @type {any} */
  let tree;
  await renderer.act(() => {
    tree = renderer.create(
      React.createElement(safe.SafeAreaProvider, null, React.createElement(Probe))
    );
  });
  assert.equal(observed.insets.top, 44);
  assert.equal(observed.frame.height, 600);
  assert.equal(
    flatten(tree.root.findByProps({ testID: 'safe' }).findByType('View').props.style).paddingTop,
    48
  );
  const live = [...handles].filter((h) => h.active);
  assert.equal(live.length, 1, 'render-time snapshot handles must be disposed');
  snapshot = {
    contentWidth: 900,
    contentHeight: 656,
    insets: { top: 0, right: 0, bottom: 0, left: 0 },
  };
  await renderer.act(() => {
    for (const h of handles) h.listener?.(snapshot);
  });
  assert.equal(observed.insets.top, 0);
  assert.equal(observed.frame.width, 900);
  assert.equal(
    flatten(tree.root.findByProps({ testID: 'safe' }).findByType('View').props.style).paddingTop,
    4
  );
  await renderer.act(() => tree.unmount());
  assert.equal([...handles].filter((h) => h.active).length, 0);
  const core = load('packages/react-native-qnx/src/SafeAreaView.js').default;
  snapshot = {
    contentWidth: 800,
    contentHeight: 600,
    insets: { top: 44, right: 0, bottom: 0, left: 0 },
  };
  await renderer.act(() => {
    tree = renderer.create(React.createElement(core, { style: { padding: 3 } }, 'core'));
  });
  assert.equal(flatten(tree.root.findByType('View').props.style).paddingTop, 47);
  await renderer.act(() => tree.unmount());
  assert.equal([...handles].filter((h) => h.active).length, 0);
  delete globals.__rnqnxWindowHost;
  await renderer.act(() => {
    tree = renderer.create(
      React.createElement(safe.SafeAreaProvider, null, React.createElement(Probe))
    );
  });
  assert.equal(observed.insets.top, 0);
  assert.equal(observed.frame.width, 700);
  await renderer.act(() => tree.unmount());
  // Effect setup must release native resources when either subscription or
  // the render-to-subscription snapshot check fails.
  for (const failure of ['listener', 'snapshot']) {
    /** @type {any[]} */
    const failedHandles = [];
    globals.__rnqnxWindowHost = {
      getCurrentWindow() {
        const handle = {
          active: true,
          subscribed: false,
          get snapshot() {
            if (failure === 'snapshot' && this.subscribed) throw new Error('snapshot setup failed');
            return snapshot;
          },
          addSnapshotListener() {
            if (failure === 'listener') throw new Error('listener setup failed');
            this.subscribed = true;
            return {
              remove: () => {
                this.subscribed = false;
              },
            };
          },
          dispose() {
            this.active = false;
          },
        };
        failedHandles.push(handle);
        return handle;
      },
    };
    const stderr = console.error;
    console.error = () => {};
    try {
      await assert.rejects(
        async () =>
          renderer.act(() => {
            tree = renderer.create(
              React.createElement(safe.SafeAreaProvider, null, React.createElement(Probe))
            );
          }),
        /setup failed/
      );
    } finally {
      console.error = stderr;
    }
    assert.ok(failedHandles.every((handle) => !handle.active && !handle.subscribed));
  }
  delete globals.__rnqnxWindowHost;
  const { insetStyle } = native.__unstableQnxWindowMetrics;
  assert.equal(
    insetStyle({ marginTop: 50 }, { top: 44 }, { top: 'maximum' }, 'margin').marginTop,
    50
  );
  assert.deepEqual(insetStyle({ paddingTop: 4 }, { top: 44 }, []), {});
  assert.throws(() => insetStyle({ paddingTop: '10%' }, { top: 44 }, ['top']), /numeric/);
} finally {
  delete globals.__rnqnxWindowHost;
  Module._load = original;
}
console.log(
  'window-insets: actual provider/core/shim rendering, native changes, additive edges and cleanup pass (host React fixture)'
);
