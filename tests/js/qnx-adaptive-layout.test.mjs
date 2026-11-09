import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';
import vm from 'node:vm';

const require = createRequire(import.meta.url);
const React = require('react');
const directory = path.resolve('packages/react-native-qnx/adaptive');
const { getWindowSizeClass, getGridLayout } = require(path.join(directory, 'layout.js'));

for (const [width, expected] of [
  [0, 'compact'],
  [599.9, 'compact'],
  [600, 'medium'],
  [839.9, 'medium'],
  [840, 'expanded'],
  [1199.9, 'expanded'],
  [1200, 'large'],
  [1599.9, 'large'],
  [1600, 'extra-large'],
]) {
  assert.equal(getWindowSizeClass({ width, height: 400 }).width, expected);
}
for (const [height, expected] of [
  [0, 'compact'],
  [479.9, 'compact'],
  [480, 'medium'],
  [899.9, 'medium'],
  [900, 'expanded'],
]) {
  assert.equal(getWindowSizeClass({ width: 1920, height }).height, expected);
}
for (const bad of [-1, NaN, Infinity, -Infinity, '600', undefined, null]) {
  assert.throws(() => getWindowSizeClass({ width: bad, height: 400 }), RangeError);
  assert.throws(() => getWindowSizeClass({ width: 800, height: bad }), RangeError);
  assert.throws(() => getGridLayout({ width: bad }), RangeError);
}
for (const width of [0, 280, 360, 615.9, 616, 840, 931.9, 932, 1280, 1920, Number.MAX_VALUE]) {
  const grid = getGridLayout({ width });
  assert.ok(grid.columns >= 1 && grid.columns <= 3);
  assert.ok(Number.isFinite(grid.itemWidth) && grid.itemWidth >= 0);
  assert.ok(
    grid.itemWidth * grid.columns + 16 * (grid.columns - 1) <= width + Math.max(1, width) * 1e-12
  );
}
assert.deepEqual(getGridLayout({ width: 280 }), { columns: 1, itemWidth: 280 });
assert.deepEqual(getGridLayout({ width: 616 }), { columns: 2, itemWidth: 300 });
assert.deepEqual(getGridLayout({ width: 932 }), { columns: 3, itemWidth: 300 });
assert.equal(getGridLayout({ width: 1920, maxColumns: 2 }).columns, 2);
assert.equal(getGridLayout({ width: Number.MAX_VALUE, gap: Number.MAX_VALUE }).columns, 1);
for (const options of [
  { width: 800, gap: -1 },
  { width: 800, gap: Infinity },
  { width: 800, minItemWidth: 0 },
  { width: 800, minItemWidth: NaN },
  ...[0, 4, 1.5, '2', NaN].map((maxColumns) => ({ width: 800, maxColumns })),
]) {
  assert.throws(() => getGridLayout(options), RangeError);
}

// Exercise the actual grid component using React elements. Measurement is
// supplied by a host fixture; this proves stable ancestry, not native caret input.
let measuredWidth = 360;
const gridModule = { exports: {} };
vm.runInNewContext(await readFile(path.join(directory, 'AdaptiveGrid.js'), 'utf8'), {
  module: gridModule,
  /** @param {string} name */
  require(name) {
    if (name === 'react') return React;
    if (name === 'react-native') return { View: 'View' };
    if (name === './layout') return { getGridLayout };
    if (name === './state')
      return { useElementSize: () => ({ width: measuredWidth, height: 100, onLayout() {} }) };
    throw new Error('Unexpected grid dependency: ' + name);
  },
});
/** @type {any} */
const { AdaptiveGrid } = gridModule.exports;
const children = ['input', 'switch', 'button'].map((key) =>
  React.createElement('Input', { key, testID: key })
);
/** @type {unknown} */
let initialIdentity;
for (const width of [360, 800, 1200, 320]) {
  measuredWidth = width;
  const tree = AdaptiveGrid({ children, gap: 16 });
  const items = tree.props.children.props.children;
  const identity = items.map((/** @type {any} */ item) => [
    item.key,
    item.type,
    item.props.children.key,
    item.props.children.type,
  ]);
  if (initialIdentity === undefined) initialIdentity = identity;
  else
    assert.deepEqual(
      identity,
      initialIdentity,
      'Resizing must not replace keyed parents or inputs'
    );
  assert.ok(items.every((/** @type {any} */ item) => item.props.style.width <= width));
}

// A deterministic commit/effect harness verifies focus commands on menu
// transitions. Native delivery is separately covered by the Qt gallery test.
/** @type {Array<{current:any}>} */
const refs = [];
/** @type {Array<unknown[] | undefined>} */
const dependencies = [];
/** @type {Array<() => void>} */
let effects = [];
let hook = 0;
let windowWidth = 600;
let selectedId = 'components';
let menuState = { menuOpen: false };
/** @type {string[]} */
const focused = [];
const menuStore = {
  setState: (/** @type {{menuOpen:boolean}} */ next) => {
    menuState = next;
  },
};
const shellModule = { exports: {} };
const hookReact = {
  ...React,
  useRef(/** @type {any} */ value) {
    const index = hook++;
    return refs[index] ?? (refs[index] = { current: value });
  },
  useEffect(/** @type {() => void} */ effect, /** @type {unknown[]} */ deps) {
    const index = hook++;
    const previous = dependencies[index];
    if (!previous || deps.some((item, i) => !Object.is(item, previous[i]))) effects.push(effect);
    dependencies[index] = deps;
  },
};
vm.runInNewContext(await readFile(path.join(directory, 'AdaptiveShell.js'), 'utf8'), {
  module: shellModule,
  /** @param {string} name */
  require(name) {
    if (name === 'react') return hookReact;
    if (name === 'react-native')
      return {
        View: 'View',
        Text: 'Text',
        Pressable: 'Pressable',
        ScrollView: 'ScrollView',
        Modal: 'Modal',
        useWindowDimensions: () => ({ width: windowWidth, height: 400 }),
      };
    if (name === './layout') return require(path.join(directory, 'layout.js'));
    if (name === './state')
      return {
        useElementSize: () => ({ width: windowWidth, height: 400, onLayout() {} }),
        useLocalStore: () => [menuState, menuStore],
      };
    throw new Error('Unexpected shell dependency: ' + name);
  },
});
/** @type {any} */
const { AdaptiveShell } = shellModule.exports;
/** @type {any[]} */
let elements = [];
/** @param {any} element @param {boolean} [inDialog] */
function attach(element, inDialog = false) {
  if (!element || typeof element !== 'object') return;
  elements.push(element);
  const modal = element.type === 'Modal';
  const label = element.props.accessibilityLabel;
  const instance = { focus: () => focused.push((inDialog ? 'dialog:' : 'page:') + label) };
  if (typeof element.props.ref === 'function') element.props.ref(instance);
  else if (element.props.ref) element.props.ref.current = instance;
  if (modal && !element.props.visible) return;
  React.Children.forEach(element.props.children, (/** @type {any} */ child) =>
    attach(child, inDialog || modal)
  );
}
/** @param {() => void} [beforeEffects] */
function renderShell(beforeEffects) {
  hook = 0;
  effects = [];
  elements = [];
  const tree = AdaptiveShell({
    title: 'Example',
    navigationLabel: 'Browse sections',
    selectedId,
    navigation: [
      { id: 'components', label: 'Components' },
      { id: 'documents', label: 'Documents' },
    ],
    onSelect: (/** @type {string} */ id) => {
      selectedId = id;
    },
  });
  attach(tree);
  beforeEffects?.();
  effects.forEach((effect) => effect());
}
/** @param {string} label */
const button = (label) =>
  elements.find(
    (element) => element.type === 'Pressable' && element.props.accessibilityLabel === label
  );
renderShell();
assert.deepEqual(focused, [], 'Initial render must not steal focus');
button('Browse sections').props.onPress();
renderShell();
assert.equal(focused.at(-1), 'dialog:Components');
elements.find((element) => element.type === 'Modal').props.onRequestClose();
renderShell();
assert.equal(
  focused.at(-1),
  'page:Browse sections',
  'Escape restoration occurs after the hidden overlay commit'
);
button('Browse sections').props.onPress();
renderShell();
elements
  .filter(
    (element) => element.type === 'Pressable' && element.props.accessibilityLabel === 'Documents'
  )
  .at(-1)
  .props.onPress();
renderShell();
assert.equal(selectedId, 'documents');
assert.equal(focused.at(-1), 'page:Browse sections');
button('Browse sections').props.onPress();
renderShell();
windowWidth = 1200;
renderShell();
assert.equal(menuState.menuOpen, false);
assert.equal(focused.at(-1), 'page:Documents');
const previouslyFocused = button('Documents');
previouslyFocused.props.onFocus();
windowWidth = 600;
renderShell(() => previouslyFocused.props.onBlur());
assert.equal(
  focused.at(-1),
  'page:Browse sections',
  'A native hide/blur must not lose the pre-resize focus owner'
);

console.log(
  'Adaptive layout: logical breakpoints, bounded grid sizing, stable element ancestry, and menu focus transitions passed (JS host fixture; no native input claim).'
);
