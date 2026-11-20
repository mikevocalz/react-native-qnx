/**
 * `StatusBar` for Marathon.
 *
 * Marathon-Shell owns the system bars: it is the Wayland compositor, and an
 * xdg-shell client has no protocol to hide, tint or resize them
 * (https://github.com/patrickjquinn/Marathon-Shell). This component therefore
 * does not claim to change anything on screen. It keeps the same public API as
 * React Native's StatusBar and records what the app asked for, so code that
 * mounts `<StatusBar hidden />` or calls `StatusBar.setBarStyle(...)` runs and
 * a host can read the request with `StatusBar.getRequestedState()` when a
 * shell API exists to honor it. At Marathon-Shell `1f845160`
 * `marathon_surface_v1.set_system_bars` is recorded and ignored, and insets
 * are always zero (docs/marathon/SHELL_CONTRACTS.md). The first call of each setter logs one
 * development warning so the missing effect is visible, not silent.
 */
'use strict';

const React = require('react');

const WARNED = new Set();

/** @param {string} name */
function warnNotApplied(name) {
  if (globalThis.__DEV__ !== true || WARNED.has(name)) {
    return;
  }
  WARNED.add(name);
  console.warn(
    `StatusBar.${name} is recorded but not applied: the Marathon shell owns the system bars.`
  );
}

const defaults = {
  hidden: false,
  barStyle: 'default',
  backgroundColor: 'black',
  translucent: false,
  networkActivityIndicatorVisible: false,
};

/** @type {Array<{ props: Record<string, unknown> }>} */
const stack = [];

function StatusBar(props) {
  const entryRef = React.useRef(null);
  React.useEffect(() => {
    entryRef.current = StatusBar.pushStackEntry(props);
    return () => {
      StatusBar.popStackEntry(entryRef.current);
      entryRef.current = null;
    };
    // Mount and unmount only; updates go through replaceStackEntry below.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);
  React.useEffect(() => {
    if (entryRef.current !== null) {
      entryRef.current = StatusBar.replaceStackEntry(entryRef.current, props);
    }
  });
  return null;
}

StatusBar.setHidden = function (hidden) {
  warnNotApplied('setHidden');
  defaults.hidden = Boolean(hidden);
};
StatusBar.setBarStyle = function (style) {
  warnNotApplied('setBarStyle');
  defaults.barStyle = style;
};
StatusBar.setBackgroundColor = function (color) {
  warnNotApplied('setBackgroundColor');
  defaults.backgroundColor = color;
};
StatusBar.setTranslucent = function (translucent) {
  warnNotApplied('setTranslucent');
  defaults.translucent = Boolean(translucent);
};
StatusBar.setNetworkActivityIndicatorVisible = function (visible) {
  warnNotApplied('setNetworkActivityIndicatorVisible');
  defaults.networkActivityIndicatorVisible = Boolean(visible);
};

StatusBar.pushStackEntry = function (props) {
  const entry = { props: { ...props } };
  stack.push(entry);
  return entry;
};
StatusBar.popStackEntry = function (entry) {
  const index = stack.indexOf(entry);
  if (index !== -1) {
    stack.splice(index, 1);
  }
};
StatusBar.replaceStackEntry = function (entry, props) {
  const next = { props: { ...props } };
  const index = stack.indexOf(entry);
  if (index !== -1) {
    stack[index] = next;
  } else {
    stack.push(next);
  }
  return next;
};

/**
 * The merged request: static setters first, then mounted components in mount
 * order, later entries winning, as in React Native.
 * @returns {typeof defaults}
 */
StatusBar.getRequestedState = function () {
  const merged = { ...defaults };
  for (const { props } of stack) {
    for (const key of Object.keys(defaults)) {
      if (props[key] !== undefined && props[key] !== null) {
        merged[key] = props[key];
      }
    }
  }
  return merged;
};

// The app surface does not include the shell's bars, so there is no height to
// inset by. Real insets must come from the host, not from this value.
StatusBar.currentHeight = 0;

module.exports = StatusBar;
module.exports.default = StatusBar;
