import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import vm from 'node:vm';

const root = process.cwd();
const exportsSource = await readFile(
  path.join(root, 'packages/react-native-qnx/Libraries/ReactNative/PublicAPI.js'),
  'utf8'
);
/** @type {string[]} */
const loaded = [];
const values = { component: true };
const module = { exports: /** @type {any} */ ({}) };
vm.runInNewContext(exportsSource, {
  module,
  require: (/** @type {string} */ name) => {
    loaded.push(name);
    return { default: values, AppRegistry: values, findNodeHandle: values };
  },
});
assert.equal(loaded.length, 0, 'Public imports stay lazy until read');
const api = module.exports;
const expected = [
  'AppRegistry',
  'AppState',
  'View',
  'Text',
  'Image',
  'ScrollView',
  'FlatList',
  'SectionList',
  'TextInput',
  'Pressable',
  'ActivityIndicator',
  'Switch',
  'Modal',
  'StyleSheet',
  'Platform',
  'Animated',
  'I18nManager',
  'findNodeHandle',
  'Dimensions',
  'PixelRatio',
  'useWindowDimensions',
  'DevSettings',
];
assert.deepEqual(Object.keys(api), expected);
for (const name of expected) assert.equal(api[name], values);
assert.equal(loaded.length, expected.length);
for (const name of ['Alert', 'BackHandler', 'NonexistentExport']) {
  assert.throws(() => api[name], /not implemented/);
}
assert.equal(api.__esModule, undefined);
assert.equal(api.default, undefined);
assert.equal(api.then, undefined);
assert.equal(api[Symbol.toStringTag], undefined);
assert.ok(Object.isFrozen(api));

const resolverModule = { exports: /** @type {any} */ ({}) };
const resolverSource = await readFile(
  path.join(root, 'packages/metro-config-qnx/public-entry.js'),
  'utf8'
);
vm.runInNewContext(resolverSource, {
  module: resolverModule,
  require: (/** @type {string} */ name) => {
    if (name === 'node:path') return path;
    if (name === '@rn-qnx/react-native-qnx') return { OVERRIDES_ROOT: '/fixture/Libraries' };
    throw new Error(`Unexpected dependency ${name}`);
  },
});
const wrap = resolverModule.exports;
const fallback = () => ({ type: 'sourceFile', filePath: '/original.js' });
const original = { resolver: { resolveRequest: fallback, platforms: ['ios', 'qnx', 'marathon'] } };
for (const platform of ['qnx', 'marathon']) {
  const config = wrap(original, platform);
  assert.notEqual(config, original);
  assert.equal(original.resolver.resolveRequest, fallback);
  assert.equal(
    config.resolver.resolveRequest({}, 'react-native', platform).filePath,
    '/fixture/Libraries/ReactNative/PublicAPI.js'
  );
  for (const request of ['react-native/Libraries/Text/Text', 'react', './Platform']) {
    assert.equal(config.resolver.resolveRequest({}, request, platform).filePath, '/original.js');
  }
  for (const other of ['ios', 'android', platform === 'qnx' ? 'marathon' : 'qnx']) {
    assert.equal(
      config.resolver.resolveRequest({}, 'react-native', other).filePath,
      '/original.js'
    );
  }
}
assert.throws(() => wrap(original, 'linux'), /qnx or marathon/);
assert.equal(
  wrap({}).resolver.resolveRequest({ resolveRequest: fallback }, 'react', 'qnx').filePath,
  '/original.js'
);

// Exercise the actual QNX Switch wrapper at its native-component boundary.
// Interpret style values using RN's documented nested-array precedence, so
// this checks the caller-visible styles rather than one output representation.
/** @param {unknown} style @returns {Record<string, unknown>} */
function flattenStyle(style) {
  if (Array.isArray(style)) return Object.assign({}, ...style.map(flattenStyle));
  return style && typeof style === 'object'
    ? Object.assign({}, /** @type {Record<string, unknown>} */ (style))
    : {};
}
const switchModule = { exports: /** @type {any} */ ({}) };
const nativeSwitch = Symbol('SwitchNativeComponent');
const switchSource = await readFile(
  path.join(root, 'packages/react-native-qnx/Libraries/Components/Switch/Switch.js'),
  'utf8'
);
vm.runInNewContext(switchSource, {
  module: switchModule,
  require: (/** @type {string} */ name) => {
    if (name === 'react') {
      return {
        forwardRef: (/** @type {Function} */ render) => render,
        createElement: (/** @type {unknown} */ type, /** @type {any} */ props) => ({ type, props }),
      };
    }
    if (name === 'react-native/Libraries/Components/Switch/SwitchNativeComponent') {
      return { default: nativeSwitch };
    }
    throw new Error(`Unexpected Switch dependency ${name}`);
  },
});
const renderSwitch = switchModule.exports;
const customStyle = Object.freeze({ alignSelf: 'center', marginLeft: 12 });
const layeredStyle = Object.freeze([
  Object.freeze({ marginLeft: 4, opacity: 0.5 }),
  false,
  Object.freeze([null, customStyle]),
]);
for (const [style, expectedStyle] of [
  [undefined, { alignSelf: 'flex-start' }],
  [customStyle, { alignSelf: 'center', marginLeft: 12 }],
  [layeredStyle, { alignSelf: 'center', marginLeft: 12, opacity: 0.5 }],
]) {
  const element = renderSwitch({ style }, null);
  assert.equal(element.type, nativeSwitch);
  assert.deepEqual(flattenStyle(element.props.style), expectedStyle);
}
const switchEvents = /** @type {unknown[]} */ ([]);
const ref = {};
const switchElement = renderSwitch(
  {
    value: false,
    disabled: true,
    style: layeredStyle,
    trackColor: { false: '#112233', true: '#445566' },
    thumbColor: '#ffffff',
    onChange: (/** @type {unknown} */ event) => switchEvents.push(event),
    onValueChange: (/** @type {boolean} */ value) => switchEvents.push(value),
  },
  ref
);
const switchEvent = { nativeEvent: { value: true } };
switchElement.props.onChange(switchEvent);
assert.deepEqual(switchEvents, [switchEvent, true]);
assert.equal(switchElement.props.ref, ref);
assert.equal(switchElement.props.disabled, true);
assert.equal(switchElement.props.value, false);
assert.equal(switchElement.props.tintColor, '#112233');
assert.equal(switchElement.props.onTintColor, '#445566');
assert.equal(switchElement.props.thumbTintColor, '#ffffff');

// Import the real QNX Image through the real public getter. The native loader
// is controlled here; decoding/networking belong to the Qt/Hermes host tests.
const imageCalls = /** @type {unknown[][]} */ ([]);
const imageFailure = new Error('image decode failed');
const cacheStatuses = Object.freeze({ 'https://example.test/cached.png': 'memory' });
const nativeCachePromise = Promise.resolve(cacheStatuses);
const imageLoader = /** @type {Record<string, (...args: any[]) => unknown>} */ ({
  getSize(uri) {
    imageCalls.push(['getSize', uri]);
    return uri === 'broken' ? Promise.reject(imageFailure) : Promise.resolve([23, 17]);
  },
  getSizeWithHeaders(uri, headers) {
    imageCalls.push(['getSizeWithHeaders', uri, headers]);
    return uri === 'broken'
      ? Promise.reject(imageFailure)
      : Promise.resolve({ width: 23, height: 17 });
  },
  prefetchImage(uri) {
    imageCalls.push(['prefetchImage', uri]);
    return uri === 'broken' ? Promise.reject(imageFailure) : Promise.resolve(true);
  },
  prefetchImageWithMetadata(uri, queryRootName, rootTag) {
    imageCalls.push(['prefetchImageWithMetadata', uri, queryRootName, rootTag]);
    return Promise.resolve(true);
  },
  queryCache(uris) {
    imageCalls.push(['queryCache', uris]);
    return nativeCachePromise;
  },
});
const imageModule = { exports: /** @type {any} */ ({}) };
const imageWarnings = /** @type {string[]} */ ([]);
let imageNativeLoads = 0;
let imageLoaderLookupFailure = /** @type {Error | undefined} */ (undefined);
const resolveAssetSource = (/** @type {unknown} */ source) => source;
vm.runInNewContext(
  await readFile(path.join(root, 'packages/react-native-qnx/Libraries/Image/Image.js'), 'utf8'),
  {
    module: imageModule,
    console: { warn: (/** @type {string} */ message) => imageWarnings.push(message) },
    require: (/** @type {string} */ name) => {
      if (name === 'react') return { createElement: () => ({}) };
      if (name === 'react-native/Libraries/Image/ImageViewNativeComponent')
        return { default: Symbol('ImageViewNativeComponent') };
      if (name === 'react-native/Libraries/Image/resolveAssetSource')
        return { default: resolveAssetSource };
      if (name === 'react-native/Libraries/Image/NativeImageLoaderIOS') {
        imageNativeLoads++;
        if (imageLoaderLookupFailure) throw imageLoaderLookupFailure;
        return { default: imageLoader };
      }
      throw new Error(`Unexpected Image dependency ${name}`);
    },
  }
);
const imagePublicModule = { exports: /** @type {any} */ ({}) };
vm.runInNewContext(exportsSource, {
  module: imagePublicModule,
  require: (/** @type {string} */ name) => {
    assert.equal(name, 'react-native/Libraries/Image/Image');
    return { default: imageModule.exports };
  },
});
const { Image } = imagePublicModule.exports;
assert.equal(Image.resolveAssetSource, resolveAssetSource);
assert.equal(imageNativeLoads, 0, 'Reading/rendering Image must not require the loader');
Image({ source: { uri: 'data:image/png;base64,fixture' } });
assert.equal(imageNativeLoads, 0);
const dimensions = await Image.getSize('https://example.test/image.png');
assert.equal(dimensions.width, 23);
assert.equal(dimensions.height, 17);
assert.deepEqual(imageCalls.pop(), ['getSize', 'https://example.test/image.png']);
const headers = Object.freeze({ 'X-Image-Test': 'header-value' });
const headerDimensions = await Image.getSizeWithHeaders('https://example.test/image.png', headers);
assert.deepEqual(headerDimensions, { width: 23, height: 17 });
assert.deepEqual(imageCalls.pop(), [
  'getSizeWithHeaders',
  'https://example.test/image.png',
  headers,
]);
await assert.rejects(Image.getSize('broken'), (error) => error === imageFailure);
await assert.rejects(
  Image.getSizeWithHeaders('broken', headers),
  (error) => error === imageFailure
);

// RN 0.88.1 retains callback overloads alongside the Promise-returning API.
for (const readSize of [
  (
    /** @type {string} */ uri,
    /** @type {(width: number, height: number) => void} */ success,
    /** @type {((error: unknown) => void) | undefined} */ failure
  ) => Image.getSize(uri, success, failure),
  (
    /** @type {string} */ uri,
    /** @type {(width: number, height: number) => void} */ success,
    /** @type {((error: unknown) => void) | undefined} */ failure
  ) => Image.getSizeWithHeaders(uri, headers, success, failure),
]) {
  const callbackValues = /** @type {unknown[]} */ ([]);
  assert.equal(
    readSize('valid', (...args) => callbackValues.push(...args), undefined),
    undefined
  );
  await new Promise((resolve) => setImmediate(resolve));
  assert.deepEqual(callbackValues, [23, 17]);
  const failures = /** @type {unknown[]} */ ([]);
  readSize(
    'broken',
    () => assert.fail('Success callback on failure'),
    (error) => failures.push(error)
  );
  await new Promise((resolve) => setImmediate(resolve));
  assert.deepEqual(failures, [imageFailure]);
  readSize('broken', () => assert.fail('Success callback on failure'), undefined);
  await new Promise((resolve) => setImmediate(resolve));
  assert.equal(imageWarnings.pop(), 'Failed to get size for image: broken');
}
assert.equal(await Image.prefetch('https://example.test/image.png'), true);
assert.deepEqual(imageCalls.pop(), ['prefetchImage', 'https://example.test/image.png']);
await assert.rejects(Image.prefetch('broken'), (error) => error === imageFailure);
assert.equal(await Image.prefetchWithMetadata('valid', 'gallery'), true);
assert.deepEqual(imageCalls.pop(), ['prefetchImageWithMetadata', 'valid', 'gallery', 0]);
assert.equal(await Image.prefetchWithMetadata('valid', 'gallery', 42), true);
assert.deepEqual(imageCalls.pop(), ['prefetchImageWithMetadata', 'valid', 'gallery', 42]);
delete imageLoader.prefetchImageWithMetadata;
assert.equal(await Image.prefetchWithMetadata('valid', 'gallery'), true);
assert.deepEqual(imageCalls.pop(), ['prefetchImage', 'valid']);
const cacheUris = Object.freeze([
  'https://example.test/cached.png',
  'https://example.test/cached.png',
  'https://example.test/missing.png',
]);
const queryCallsBefore = imageCalls.length;
const cachePromise = Image.queryCache(cacheUris);
assert.equal(imageCalls.length, queryCallsBefore + 1, 'Native cache lookup stays immediate');
assert.equal(imageCalls.at(-1)?.[1], cacheUris, 'URI order and duplicates are forwarded intact');
assert.notEqual(cachePromise, nativeCachePromise, 'RN queryCache returns a fresh Promise');
assert.equal(await cachePromise, cacheStatuses, 'Native cache values are not reshaped');
assert.deepEqual(imageCalls.pop(), ['queryCache', cacheUris]);

// The wrapper preserves the upstream async function's settlement contract even
// if module loading/native invocation throws, or a loader returns a thenable.
for (const value of [
  cacheStatuses,
  {},
  {
    then: (/** @type {(value: unknown) => void} */ resolve) => resolve(cacheStatuses),
  },
]) {
  imageLoader.queryCache = () => value;
  const queried = Image.queryCache(cacheUris);
  assert.equal(typeof queried.then, 'function');
  assert.equal(await queried, 'then' in value ? cacheStatuses : value);
}
const queryFailure = new Error('queryCache failed');
for (const query of [
  () => {
    throw queryFailure;
  },
  () => Promise.reject(queryFailure),
  () => ({
    get then() {
      throw queryFailure;
    },
  }),
]) {
  imageLoader.queryCache = query;
  const rejected = Image.queryCache(cacheUris); // A synchronous throw would fail here.
  await assert.rejects(rejected, (error) => error === queryFailure);
}
imageLoaderLookupFailure = new Error('ImageLoader unavailable');
const missingLoader = Image.queryCache(cacheUris);
await assert.rejects(missingLoader, (error) => error === imageLoaderLookupFailure);
console.log(
  'public-entry exports/resolver, Switch styles/events and Image loader contracts passed (host VM only)'
);
