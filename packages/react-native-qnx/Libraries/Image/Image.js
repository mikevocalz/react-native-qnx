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
 * side expects. What the iOS and Android versions add and this does not:
 * `Image.getSize`, `prefetch`, and the load lifecycle events. Those are
 * recorded in `docs/component-support.md` rather than stubbed here.
 */

'use strict';

const React = require('react');
const ImageViewNativeComponent =
  require('react-native/Libraries/Image/ImageViewNativeComponent').default;
const resolveAssetSource = require('react-native/Libraries/Image/resolveAssetSource').default;

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
Image.resolveAssetSource = resolveAssetSource;

module.exports = Image;
module.exports.default = Image;
