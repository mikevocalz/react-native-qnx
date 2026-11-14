// @ts-nocheck: test harness around untyped Babel, Yoga and react-test-renderer.
// Loads the component gallery's real route and screen files in plain Node:
// Babel turns their ESM and JSX into CommonJS, `react-native` and the native
// Expo packages are replaced by host-element stand-ins that keep every prop,
// and `require.context` (Metro only) lists the directory. Nothing in
// apps/component-gallery is changed or copied.
const fs = require('node:fs');
const Module = require('node:module');
const path = require('node:path');
const babel = require('@babel/core');
const React = require('react');
const base = require('./mock-react-native.cjs');

const root = path.resolve(__dirname, '../../..');
const galleryRoot = path.join(root, 'apps/component-gallery') + path.sep;
const routerSrc = path.join(root, 'packages/expo-router-qnx/src') + path.sep;

const h = React.createElement;
const View = base.View;
const host = (/** @type {string} */ name) => {
  const Component = React.forwardRef((/** @type {any} */ props, ref) => h(name, { ...props, ref }));
  Component.displayName = name;
  return Component;
};

// Pressable resolves function styles and function children the way RN does.
const Pressable = React.forwardRef((/** @type {any} */ { style, children, ...rest }, ref) => {
  const state = { pressed: false };
  return h('Pressable', {
    ...rest,
    ref,
    style: typeof style === 'function' ? style(state) : style,
    children: typeof children === 'function' ? children(state) : children,
  });
});

// The scroll view's content container is a View of its own, as in RN.
const ScrollView = React.forwardRef(
  (/** @type {any} */ { style, contentContainerStyle, children, ...rest }, ref) =>
    h(
      'ScrollView',
      { ...rest, ref, style: [{ overflow: 'scroll' }, style] },
      h('View', { style: contentContainerStyle }, children)
    )
);

// VirtualizedList stand-in: mounts the first `initialNumToRender` rows.
const FlatList = (
  /** @type {any} */ {
    data = [],
    renderItem,
    keyExtractor,
    initialNumToRender = 10,
    numColumns = 1,
    ListHeaderComponent,
    ListFooterComponent,
    ListEmptyComponent,
    ItemSeparatorComponent,
    style,
    contentContainerStyle,
  }
) => {
  const items = data
    .slice(0, initialNumToRender * numColumns)
    .map((/** @type {any} */ item, /** @type {number} */ index) =>
      h(
        React.Fragment,
        { key: keyExtractor ? keyExtractor(item, index) : (item.key ?? index) },
        renderItem({ item, index })
      )
    );
  // numColumns > 1: each row is a flex-row View, as VirtualizedList draws it.
  const rows = [];
  for (let at = 0; at < items.length; at += numColumns) {
    const cells = items.slice(at, at + numColumns);
    const row =
      numColumns > 1
        ? h(View, { key: `row${at}`, style: { flexDirection: 'row' } }, cells)
        : cells[0];
    if (at > 0 && ItemSeparatorComponent) rows.push(h(ItemSeparatorComponent, { key: `sep${at}` }));
    rows.push(row);
  }
  const part = (/** @type {any} */ C) => (C ? (typeof C === 'function' ? h(C) : C) : null);
  return h(
    ScrollView,
    { style, contentContainerStyle },
    part(ListHeaderComponent),
    rows.length ? rows : part(ListEmptyComponent),
    part(ListFooterComponent)
  );
};

const SectionList = (
  /** @type {any} */ {
    sections = [],
    renderItem,
    renderSectionHeader,
    style,
    contentContainerStyle,
  }
) =>
  h(
    ScrollView,
    { style, contentContainerStyle },
    sections.flatMap((/** @type {any} */ section, /** @type {number} */ s) => [
      renderSectionHeader
        ? h(React.Fragment, { key: `h${s}` }, renderSectionHeader({ section }))
        : null,
      ...section.data
        .slice(0, 6)
        .map((/** @type {any} */ item, /** @type {number} */ index) =>
          h(React.Fragment, { key: `${s}.${index}` }, renderItem({ item, index, section }))
        ),
    ])
  );

const Image = host('Image');
Image.getSize = () => {};
Image.resolveAssetSource = () => ({ uri: 'file:///asset' });

const named = {
  ...base,
  Animated: {
    ...base.Animated,
    createAnimatedComponent: (/** @type {any} */ C) => C,
    // Looping animations never run in a layout test.
    loop: () => ({ start() {}, stop() {} }),
    sequence: () => ({ start() {}, stop() {} }),
    parallel: () => ({ start() {}, stop() {} }),
  },
  Easing: new Proxy({}, { get: () => (/** @type {any} */ x) => x }),
  Pressable,
  ScrollView,
  FlatList,
  SectionList,
  Image,
  TextInput: host('TextInput'),
  Switch: host('Switch'),
  ActivityIndicator: host('ActivityIndicator'),
  Modal: ({ visible, children }) => (visible ? children : null),
  DevSettings: { addMenuItem() {} },
};

const svgNames = [
  'Svg',
  'Path',
  'Circle',
  'Ellipse',
  'Rect',
  'Line',
  'Polygon',
  'Polyline',
  'G',
  'Defs',
  'LinearGradient',
  'Stop',
  'Text',
  'TSpan',
  'ClipPath',
  'Mask',
  'RadialGradient',
  'Use',
  'Pattern',
  'Symbol',
  'Marker',
  'Filter',
  'FeGaussianBlur',
  'FeOffset',
  'FeMerge',
  'FeMergeNode',
  'FeColorMatrix',
  'FeFlood',
  'FeComposite',
  'FeBlend',
  'FeMorphology',
  'FeTurbulence',
  'FeDisplacementMap',
  'FeComponentTransfer',
  'FeFuncR',
  'FeFuncG',
  'FeFuncB',
  'FeDropShadow',
  'FeConvolveMatrix',
  'FeTile',
  'FeDiffuseLighting',
  'FeSpecularLighting',
  'FeDistantLight',
  'FePointLight',
  'ForeignObject',
  'Image',
  'TextPath',
  'SvgXml',
  'SvgAst',
];
const svg = Object.fromEntries(
  svgNames.map((name) => [name, host(name === 'Text' ? 'SvgText' : name)])
);
svg.default = svg.Svg;
// Layout-only stand-in; SVG parsing/rendering is exercised by native corpus tests.
svg.parse = () => ({});

const stubs = {
  'react-native': named,
  'react-native/Libraries/ReactNative/UIManager': { default: { hasViewManagerConfig: () => true } },
  'react-native-svg': { __esModule: true, ...svg },
  'react-native-svg/css': { SvgCss: host('SvgCss') },
  'expo-image': { __esModule: true, Image },
  'expo-constants': {
    __esModule: true,
    default: {
      expoConfig: { name: 'Gallery', slug: 'gallery' },
      platform: { qnx: { systemName: 'QNX', systemVersion: '8.0.5', machine: 'aarch64' } },
    },
  },
  'expo-router': {
    __esModule: true,
    Link: ({ children }) => children,
    useRouter: () => ({ push() {}, navigate() {} }),
    usePathname: () => '/',
    useNavigation: () => ({ dispatch() {} }),
  },
  'expo-router/react-navigation': {
    DrawerActions: { openDrawer: () => ({}), toggleDrawer: () => ({}) },
  },
};

function install() {
  const originalLoad = Module._load;
  Module._load = function load(request, parent, isMain) {
    if (Object.hasOwn(stubs, request)) return stubs[request];
    // The Qt native components (video, svg document) are generated TypeScript specs.
    if (request.endsWith('packages/react-native-qnx/qt'))
      return { QtVideo: host('QtVideo'), QtSvgDocument: host('QtSvgDocument') };
    return originalLoad.call(this, request, parent, isMain);
  };
  const originalJs = Module._extensions['.js'];
  Module._extensions['.js'] = function compile(module, filename) {
    if (!filename.startsWith(galleryRoot) || filename.includes('node_modules'))
      return originalJs(module, filename);
    const source = fs
      .readFileSync(filename, 'utf8')
      .replace(/\brequire\.context\(/g, 'globalThis.__requireContext(__dirname, ');
    const { code } = babel.transformSync(source, {
      filename,
      babelrc: false,
      configFile: false,
      sourceType: 'unambiguous',
      plugins: [
        [require.resolve('@babel/plugin-transform-modules-commonjs'), { loose: true }],
        require.resolve('@babel/plugin-transform-react-jsx'),
      ],
    });
    return module._compile(code, filename);
  };
  const assetExtensions = ['.png', '.gif', '.webp', '.mp4', '.wav', '.mp3', '.jpg', '.jpeg'];
  for (const extension of assetExtensions)
    Module._extensions[extension] = (module) => {
      module.exports = 1;
    };
  globalThis.__DEV__ = true;
  // Metro's require.context(dir, false, /\.card\.js$/) for one directory.
  globalThis.__requireContext = (
    /** @type {string} */ dir,
    /** @type {string} */ _rel,
    /** @type {boolean} */ _deep,
    /** @type {RegExp} */ pattern
  ) => {
    const keys = fs
      .readdirSync(dir)
      .filter((name) => pattern.test(name))
      .map((name) => `./${name}`);
    const context = (/** @type {string} */ key) => require(path.join(dir, key));
    context.keys = () => keys;
    return context;
  };
  return () => {
    Module._load = originalLoad;
    Module._extensions['.js'] = originalJs;
    for (const extension of assetExtensions) delete Module._extensions[extension];
    delete globalThis.__requireContext;
    for (const key of Object.keys(require.cache)) {
      if (key.startsWith(galleryRoot) || key.startsWith(routerSrc)) delete require.cache[key];
    }
  };
}

module.exports = { install, root, galleryRoot };
