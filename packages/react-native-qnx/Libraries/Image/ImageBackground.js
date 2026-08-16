/**
 * `ImageBackground` for QNX.
 *
 * React Native's `ImageBackground` uses `StyleSheet.absoluteFill` and expects
 * the `Image` to have intrinsic content size. Qt Quick's `Image` works more
 * reliably with explicit width and height, so this override builds the same
 * View/Image/children structure with a simpler, explicit style.
 */

'use strict';

const React = require('react');
const Image = require('react-native/Libraries/Image/Image').default;
const StyleSheet = require('react-native/Libraries/StyleSheet/StyleSheet').default;
const View = require('react-native/Libraries/Components/View/View').default;

function ImageBackground(props) {
  const children = props.children;
  const imageRef = props.imageRef;
  const imageStyle = props.imageStyle;
  const style = props.style;
  const imageProps = Object.assign({}, props);
  delete imageProps.children;
  delete imageProps.imageRef;
  delete imageProps.imageStyle;
  delete imageProps.style;

  // If no explicit image style is supplied, inherit the container's dimensions
  // so the image actually paints. The user can still override with `imageStyle`.
  const resolvedImageStyle = Object.assign({}, style, imageStyle);

  imageProps.ref = imageRef;
  imageProps.style = resolvedImageStyle;

  return React.createElement(
    View,
    { style, collapsable: false },
    React.createElement(Image, imageProps),
    React.createElement(
      View,
      {
        style: StyleSheet.absoluteFill,
        collapsable: false,
      },
      children
    )
  );
}

module.exports = ImageBackground;
module.exports.default = ImageBackground;
