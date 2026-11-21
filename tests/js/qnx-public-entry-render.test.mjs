// @ts-nocheck: test harness around untyped Babel and react-test-renderer.
// Host test: constructs and renders VirtualizedList, VirtualizedSectionList,
// Button, ImageBackground and SafeAreaView through the real public entry
// (PublicAPI.js), the real upstream react-native@0.88.1 modules behind it and
// the real @react-native/virtualized-lists package. Only the native leaf
// components (View, Text, Image, ScrollView, TouchableOpacity) and the
// Platform/StyleSheet/I18nManager modules are replaced by host elements, the
// way tests/js/helpers/gallery-loader.cjs replaces them. This proves the
// wiring and the component code; it is not QNX VM or Pi evidence.
import assert from 'node:assert/strict';
import { existsSync, readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import path from 'node:path';
import vm from 'node:vm';

const root = process.cwd();
const require = createRequire(import.meta.url);
const React = require('react');
const { act, create } = require('react-test-renderer');
globalThis.IS_REACT_ACT_ENVIRONMENT = true;
globalThis.__DEV__ = true; // Metro defines this global for every bundle.

const rnRoot = path.dirname(require.resolve('react-native/package.json'));
const presetRequire = createRequire(require.resolve('babel-preset-expo'));
const babel = presetRequire('@babel/core');
const overridesRoot = path.join(root, 'packages/react-native-qnx/Libraries');
const publicApiPath = path.join(overridesRoot, 'ReactNative/PublicAPI.js');
const h = React.createElement;

// @react-native/babel-preset compiles a console.warn into every deep import of
// react-native/Libraries. The QNX sources use them on purpose (the QNX resolver
// maps them), so drop that one message and keep every other warning.
const warn = console.warn;
console.warn = (message, ...rest) => {
  if (typeof message !== 'string' || !message.startsWith('Deep imports from'))
    warn(message, ...rest);
};

const host = (name) => {
  const Component = React.forwardRef((props, ref) => h(name, { ...props, ref }));
  Component.displayName = name;
  return Component;
};
const flatten = (style) =>
  Array.isArray(style)
    ? Object.assign({}, ...style.map(flatten))
    : style && typeof style === 'object'
      ? { ...style }
      : {};
const Platform = {
  OS: 'qnx',
  select: (spec) => ('qnx' in spec ? spec.qnx : spec.default),
  constants: {},
};
const StyleSheet = {
  create: (styles) => styles,
  flatten,
  compose: (a, b) => [a, b],
  hairlineWidth: 1,
  absoluteFill: { position: 'absolute', left: 0, right: 0, top: 0, bottom: 0 },
  absoluteFillObject: { position: 'absolute', left: 0, right: 0, top: 0, bottom: 0 },
};
const nativeLeaves = {
  'Components/View/View': { default: host('View') },
  'Text/Text': { default: host('Text') },
  'Image/Image': { default: host('Image') },
  // Upstream ScrollView exposes a Context that VirtualizedList consumes.
  'Components/ScrollView/ScrollView': {
    default: Object.assign(host('ScrollView'), { Context: React.createContext(null) }),
  },
  'Components/Touchable/TouchableOpacity': { default: host('TouchableOpacity') },
  'Components/Touchable/TouchableNativeFeedback': { default: host('TouchableNativeFeedback') },
  // SafeAreaView evaluates this iOS-only require eagerly inside Platform.select.
  'Components/SafeAreaView/RCTSafeAreaViewNativeComponent': {
    default: host('RCTSafeAreaView'),
  },
  'Utilities/Platform': { default: Platform },
  'StyleSheet/StyleSheet': { default: StyleSheet },
  'ReactNative/I18nManager': { default: { isRTL: false, getConstants: () => ({ isRTL: false }) } },
};

const cache = new Map();
const compiled = new Map();
function compile(file) {
  if (file === publicApiPath) return readFileSync(file, 'utf8'); // plain CommonJS
  if (!compiled.has(file)) {
    const out = babel.transformSync(readFileSync(file, 'utf8'), {
      filename: file,
      babelrc: false,
      configFile: false,
      presets: [presetRequire.resolve('@react-native/babel-preset')],
    });
    compiled.set(file, out.code);
  }
  return compiled.get(file);
}
function resolveRequest(request, fromFile) {
  if (request === 'react-native') return publicApiPath;
  const prefix = 'react-native/Libraries/';
  if (request.startsWith(prefix)) {
    const relative = request.slice(prefix.length);
    // The QNX resolver substitutes an override when one exists at this path.
    const override = path.join(overridesRoot, `${relative}.js`);
    return existsSync(override) ? override : path.join(rnRoot, 'Libraries', `${relative}.js`);
  }
  if (request.startsWith('.')) {
    const base = path.resolve(path.dirname(fromFile), request);
    const file =
      base.endsWith('.js') && existsSync(base)
        ? base
        : existsSync(`${base}.js`)
          ? `${base}.js`
          : path.join(base, 'index.js');
    // Same substitution as the QNX resolver, for files under react-native/Libraries.
    const libraries = path.join(rnRoot, 'Libraries') + path.sep;
    if (!file.startsWith(libraries)) return file;
    const override = path.join(overridesRoot, file.slice(libraries.length));
    return existsSync(override) ? override : file;
  }
  return null;
}
function load(file) {
  if (cache.has(file)) return cache.get(file).exports;
  const module = { exports: {} };
  cache.set(file, module);
  const localRequire = (request) => {
    // virtualized-lists reads two JS feature flags. Both default to false in
    // react-native@0.88.1 (src/private/featureflags/ReactNativeFeatureFlagsBase.js).
    if (request === 'react-native/react-private-interface') {
      return {
        ReactNativeFeatureFlags: {
          deferFlatListFocusChangeRenderUpdate: () => false,
          fixVirtualizeListCollapseWindowSize: () => false,
        },
      };
    }
    const resolved = resolveRequest(request, file);
    if (resolved) {
      const libraries = resolved.startsWith(overridesRoot)
        ? overridesRoot
        : path.join(rnRoot, 'Libraries');
      const key = path.relative(libraries, resolved).replace(/\.js$/, '');
      if (nativeLeaves[key]) return { __esModule: true, ...nativeLeaves[key] };
      return load(resolved);
    }
    // @react-native/virtualized-lists ships Flow sources; everything else
    // (react, invariant, memoize-one, ...) is plain CommonJS.
    const external = createRequire(file).resolve(request);
    return external.includes(`${path.sep}@react-native${path.sep}virtualized-lists${path.sep}`)
      ? load(external)
      : createRequire(file)(request);
  };
  const wrapper = vm.runInThisContext(
    `(function (exports, require, module, __filename, __dirname) {${compile(file)}\n})`,
    { filename: file }
  );
  wrapper(module.exports, localRequire, module, file, path.dirname(file));
  return module.exports;
}

const api = load(publicApiPath);
// The module the QNX resolver picks for a react-native/Libraries path: the
// override when one exists, otherwise the installed upstream file.
const backing = (name) =>
  load(resolveRequest(`react-native/Libraries/${name}`, publicApiPath)).default;

function render(element) {
  let renderer;
  act(() => {
    renderer = create(element);
  });
  return renderer;
}
const findAll = (renderer, type) => renderer.root.findAll((node) => node.type === type);
const texts = (renderer) =>
  findAll(renderer, 'Text').map((node) => [node.props.children].flat().join(''));

// 1. Every export is the default export of the module behind it. ImageBackground
// has a QNX override (packages/react-native-qnx/Libraries/Image/ImageBackground.js);
// the other four are upstream react-native@0.88.1 files.
const cases = {
  VirtualizedList: 'Lists/VirtualizedList',
  VirtualizedSectionList: 'Lists/VirtualizedSectionList',
  Button: 'Components/Button',
  ImageBackground: 'Image/ImageBackground',
  SafeAreaView: 'Components/SafeAreaView/SafeAreaView',
};
for (const [name, modulePath] of Object.entries(cases)) {
  const component = api[name];
  assert.ok(component, `${name} is exported by the public entry`);
  assert.equal(component, backing(modulePath), `${name} is the backing module's default export`);
}

// 2. VirtualizedList mounts only the first window of rows, with the item data.
{
  const data = Array.from({ length: 200 }, (_, index) => ({
    key: String(index),
    label: `Item ${index + 1}`,
  }));
  const renderer = render(
    h(api.VirtualizedList, {
      data,
      initialNumToRender: 8,
      getItemCount: (items) => items.length,
      getItem: (items, index) => items[index],
      keyExtractor: (item) => item.key,
      renderItem: ({ item }) => h('Text', null, item.label),
    })
  );
  const rows = texts(renderer);
  assert.deepEqual(rows.slice(0, 3), ['Item 1', 'Item 2', 'Item 3']);
  assert.ok(rows.length >= 8 && rows.length < 200, `windowed render, got ${rows.length} rows`);
  assert.equal(findAll(renderer, 'ScrollView').length, 1);
}

// 3. VirtualizedSectionList renders section headers and the items under them.
{
  const sections = [
    { title: 'A', data: [{ id: 'a1' }, { id: 'a2' }] },
    { title: 'B', data: [{ id: 'b1' }] },
  ];
  const renderer = render(
    h(api.VirtualizedSectionList, {
      sections,
      getItemCount: (items) => items.length,
      getItem: (items, index) => items[index],
      keyExtractor: (item) => item.id,
      renderItem: ({ item }) => h('Text', null, `row ${item.id}`),
      renderSectionHeader: ({ section }) => h('Text', null, `header ${section.title}`),
    })
  );
  assert.deepEqual(texts(renderer), ['header A', 'row a1', 'row a2', 'header B', 'row b1']);
}

// 4. Button renders its title and reports presses; disabled blocks them.
{
  const presses = [];
  const press = render(
    h(api.Button, { title: 'Press me', onPress: () => presses.push('press'), color: '#1b8f96' })
  );
  const touchable = findAll(press, 'TouchableOpacity')[0];
  assert.ok(touchable, 'Button mounts a TouchableOpacity on qnx');
  assert.equal(touchable.props.accessibilityRole, 'button');
  assert.deepEqual(texts(press), ['Press me']);
  act(() => touchable.props.onPress({}));
  assert.deepEqual(presses, ['press']);

  const disabled = render(
    h(api.Button, { title: 'Disabled', onPress: () => presses.push('bad'), disabled: true })
  );
  const disabledTouchable = findAll(disabled, 'TouchableOpacity')[0];
  assert.equal(disabledTouchable.props.disabled, true);
  assert.equal(disabledTouchable.props.accessibilityState?.disabled, true);
}

// 5. ImageBackground (QNX override) paints the image first, sized by the
// container style, and puts the children in an overlay View above it.
{
  const source = { uri: 'file:///icon.png' };
  const style = { width: 40, height: 20 };
  const renderer = render(
    h(api.ImageBackground, { source, resizeMode: 'contain', style }, h('Text', null, 'over'))
  );
  const json = renderer.toJSON();
  assert.equal(json.type, 'View');
  const [image, overlay] = json.children;
  assert.equal(image.type, 'Image', 'image is the first child, so it paints behind');
  assert.equal(image.props.source, source);
  assert.equal(image.props.resizeMode, 'contain');
  assert.equal(image.props.style.width, 40);
  assert.equal(image.props.style.height, 20);
  assert.equal(overlay.type, 'View');
  assert.deepEqual(texts(renderer), ['over']);
}

// 6. SafeAreaView is the plain View off iOS; props and children pass through.
{
  const renderer = render(
    h(api.SafeAreaView, { testID: 'safe', style: { flex: 1 } }, h('Text', null, 'inside'))
  );
  const json = renderer.toJSON();
  assert.equal(json.type, 'View');
  assert.equal(json.props.testID, 'safe');
  assert.deepEqual(texts(renderer), ['inside']);
}

console.log(
  'public entry renders VirtualizedList, VirtualizedSectionList, Button, ImageBackground and SafeAreaView (host test renderer only)'
);
