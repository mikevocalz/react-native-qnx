// The SVG cards for the gallery's Advanced tab. One file per card; this list
// is the only registration.
const React = require('react');
const { View, Text } = require('react-native');
const UIManager = require('react-native/Libraries/ReactNative/UIManager').default;
const { INK, MUTED } = require('./theme');

const svgCards = [
  require('./PathsCard'),
  require('./GradientsCard'),
  require('./ClipMaskCard'),
  require('./TextCard'),
  require('./PatternsCard'),
  require('./StrokesCard'),
  require('./TextLayoutCard'),
  require('./FiltersCard'),
  require('./ImageCard'),
  require('./XmlCard'),
  require('./ForeignObjectCard'),
  require('./PressableCard'),
];

// react-native-svg elements only exist in a host built with
// RNQNX_ENABLE_QT_RNSVG=ON; say so instead of failing inside React.
function svgAvailable() {
  return UIManager.hasViewManagerConfig('RNSVGSvgView');
}

function SvgUnavailable() {
  return (
    <View style={{ padding: 20 }}>
      <Text style={{ color: INK, fontSize: 16 }}>react-native-svg is not in this host build</Text>
      <Text style={{ color: MUTED, fontSize: 13, lineHeight: 20 }}>
        Build the QNX host with RNQNX_ENABLE_QT_RNSVG=ON to render these cards.
      </Text>
    </View>
  );
}

module.exports = { svgCards, svgAvailable, SvgUnavailable };
