/**
 * `Image` for QNX.
 *
 * React Native resolves `Libraries/Image/Image.js` by platform extension, and
 * with no `Image.qnx.js` in its tree the import is undefined — React then throws
 * "Element type is invalid". This is the third such file, after `Platform` and
 * `BaseViewConfig`.
 *
 * Deliberately minimal. It renders React Native's own native image component
 * and resolves `source`, `defaultSource`, and `loadingIndicatorSource` through
 * React Native's `resolveAssetSource` before normalising the list the native
 * side expects. Static loader methods preserve React Native 0.88.1's
 * Promise/callback contract and call the existing QNX ImageLoader module.
 * QNX prefetch uses the backend's bounded memory cache, not a disk cache.
 * Remaining component limits are recorded in `docs/component-support.md`.
 */

'use strict';

const React = require('react');
const ImageViewNativeComponent =
  require('react-native/Libraries/Image/ImageViewNativeComponent').default;
const resolveAssetSource = require('react-native/Libraries/Image/resolveAssetSource').default;

function imageLoader() {
  // The existing QNX core adapter implements this portable ImageLoader spec.
  // Keep loading lazy so displaying an Image does not require loader methods.
  return require('react-native/Libraries/Image/NativeImageLoaderIOS').default;
}

function sizeResult(uri, promise, success, failure) {
  if (typeof success !== 'function') return promise;
  promise
    .then((size) => success(size.width, size.height))
    .catch(failure || (() => console.warn('Failed to get size for image: ' + uri)));
}

function getSize(uri, success, failure) {
  const promise = imageLoader()
    .getSize(uri)
    .then((size) => ({ width: size[0], height: size[1] }));
  return sizeResult(uri, promise, success, failure);
}

function getSizeWithHeaders(uri, headers, success, failure) {
  return sizeResult(uri, imageLoader().getSizeWithHeaders(uri, headers), success, failure);
}

function prefetch(uri) {
  return imageLoader().prefetchImage(uri);
}

function prefetchWithMetadata(uri, queryRootName, rootTag) {
  const loader = imageLoader();
  if (loader.prefetchImageWithMetadata) {
    return loader.prefetchImageWithMetadata(uri, queryRootName, rootTag == null ? 0 : rootTag);
  }
  return loader.prefetchImage(uri);
}

function queryCache(uris) {
  // Preserve RN's fresh Promise and synchronous-error rejection without an
  // async-to-generator transform requiring an undeclared Babel runtime helper.
  return new Promise((resolve) => {
    resolve(imageLoader().queryCache(uris));
  });
}

function resolveImageSource(source) {
  if (source == null) {
    return source;
  }
  return resolveAssetSource(source);
}

function resolveImageSources(source) {
  if (source == null) {
    return [];
  }
  if (Array.isArray(source)) {
    return source.map(resolveImageSource).filter(Boolean);
  }
  const resolved = resolveImageSource(source);
  return resolved == null ? [] : [resolved];
}

function Image(props) {
  // Object rest destructuring is avoided deliberately: Babel lowers it to an
  // @babel/runtime helper, and pulling that in for one line would add a runtime
  // dependency to the platform package.
  const nativeProps = Object.assign({}, props);

  // The native side reads a list so a platform can choose by scale or size;
  // a single object is the common case. Bundler assets (require('./x.png'))
  // are resolved here before the URI reaches the native image item.
  nativeProps.source = resolveImageSources(props.source);
  nativeProps.defaultSource = resolveImageSource(props.defaultSource);
  nativeProps.loadingIndicatorSource = resolveImageSource(props.loadingIndicatorSource);

  return React.createElement(ImageViewNativeComponent, nativeProps);
}

Image.displayName = 'Image';
Image.getSize = getSize;
Image.getSizeWithHeaders = getSizeWithHeaders;
Image.prefetch = prefetch;
Image.prefetchWithMetadata = prefetchWithMetadata;
Image.queryCache = queryCache;
Image.resolveAssetSource = resolveAssetSource;

module.exports = Image;
module.exports.default = Image;
