/**
 * `Image` for QNX.
 *
 * React Native resolves `Libraries/Image/Image.js` by platform extension, and
 * with no `Image.qnx.js` in its tree the import is undefined — React then throws
 * "Element type is invalid". This is the third such file, after `Platform` and
 * `BaseViewConfig`.
 *
 * Deliberately minimal. It renders React Native's own native image component
 * and normalises `source` into the list the native side expects. What the iOS
 * and Android versions add and this does not: `resolveAssetSource` for bundler
 * assets, `defaultSource`, `loadingIndicatorSource`, `Image.getSize`,
 * `prefetch`, and the load lifecycle events. Those are recorded in
 * `docs/component-support.md` rather than stubbed here.
 */

'use strict';

const React = require('react');
const ImageViewNativeComponent =
  require('react-native/Libraries/Image/ImageViewNativeComponent').default;

function Image(props) {
  // Object rest destructuring is avoided deliberately: Babel lowers it to an
  // @babel/runtime helper, and pulling that in for one line would add a runtime
  // dependency to the platform package.
  const nativeProps = Object.assign({}, props);

  // The native side reads a list so a platform can choose by scale or size;
  // a single object is the common case.
  const source = props.source;
  nativeProps.source = source == null ? [] : Array.isArray(source) ? source : [source];

  return React.createElement(ImageViewNativeComponent, nativeProps);
}

Image.displayName = 'Image';

module.exports = Image;
module.exports.default = Image;
