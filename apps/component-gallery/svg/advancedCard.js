// Turns one ./svg card into an Advanced-tab card (components/advanced/). The
// card module is required on first render, not at import, so a host built
// without react-native-svg (RNQNX_ENABLE_QT_RNSVG=OFF) never evaluates it and
// shows SvgUnavailable instead. SVG cards draw their own frame and title
// (SvgCardFrame), hence `framed: false`.
const React = require('react');

function svgAdvancedCard({ id, title, order, load }) {
  function SvgAdvancedCard() {
    const { svgAvailable, SvgUnavailable } = require('./cards');
    if (!svgAvailable()) {
      // Keep the card's frame and title so the grid still says what is missing.
      const { SvgCardFrame } = require('./SvgCardFrame');
      return React.createElement(
        SvgCardFrame,
        { title, detail: 'react-native-svg card' },
        React.createElement(SvgUnavailable)
      );
    }
    return React.createElement(load().Component);
  }
  return {
    id,
    title,
    detail: 'react-native-svg 15.16.0 on a Qt-backed renderer',
    order,
    framed: false,
    Component: SvgAdvancedCard,
  };
}

module.exports = { svgAdvancedCard };
