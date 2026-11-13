'use strict';

const React = require('react');
const UIManager = require('react-native/Libraries/ReactNative/UIManager').default;
const NativeSvgDocument = require('./QtSvgDocumentNativeComponent').default;

/**
 * Render an inline static SVG document. Requires RNQNX_ENABLE_QT_SVG=ON.
 * Yoga owns the view size; set width and height in style.
 */
function QtSvgDocument(props) {
  if (typeof props.xml !== 'string') {
    throw new TypeError('QtSvgDocument.xml must be an inline SVG string.');
  }
  if (props.resizeMode != null && !['contain', 'cover', 'stretch'].includes(props.resizeMode)) {
    throw new TypeError('QtSvgDocument.resizeMode must be contain, cover, or stretch.');
  }
  if (!UIManager.hasViewManagerConfig('QnxSvgDocument')) {
    throw new Error(
      'QtSvgDocument is unavailable. Build the QNX/Qt host with RNQNX_ENABLE_QT_SVG=ON.'
    );
  }
  if (props.children != null) {
    throw new Error('QtSvgDocument accepts inline XML, not React children.');
  }
  return React.createElement(NativeSvgDocument, { ...props, collapsable: false });
}

module.exports = { QtSvgDocument };
