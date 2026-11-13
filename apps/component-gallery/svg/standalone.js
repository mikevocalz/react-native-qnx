const React = require('react');
const { View, ScrollView, Text } = require('react-native');
const { svgCards, svgAvailable, SvgUnavailable } = require('./index');
const { INK, MUTED } = require('./theme');

// The SVG cards without the tab shell. Register it as the app's component
// (extra.rnqnx.componentName = "RNQnxSvgGallery") to open straight onto them,
// which is how the device `--screenshot` check captures them.
function SvgGalleryApp() {
  return (
    <View style={{ flex: 1, width: '100%', height: '100%', backgroundColor: '#0c1929', padding: 24 }}>
      <Text style={{ color: INK, fontSize: 24, lineHeight: 30 }}>react-native-svg on Qt Quick</Text>
      <Text style={{ color: MUTED, fontSize: 13, lineHeight: 20, marginBottom: 14 }}>
        react-native-svg 15.16.0, drawn by one QPainter pass per Svg
      </Text>
      {svgAvailable() ? (
        <ScrollView contentContainerStyle={{ paddingBottom: 24 }}>
          {svgCards.map((card) => (
            <card.Component key={card.id} />
          ))}
        </ScrollView>
      ) : (
        <SvgUnavailable />
      )}
    </View>
  );
}

module.exports = { SvgGalleryApp };
