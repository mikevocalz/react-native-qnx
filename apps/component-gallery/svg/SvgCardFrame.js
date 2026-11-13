const React = require('react');
const { View, Text } = require('react-native');
const { INK, MUTED, PANEL } = require('./theme');

// One SVG card: a title, one line on what it exercises, and the drawing.
function SvgCardFrame({ title, detail, children }) {
  return (
    <View style={{ backgroundColor: PANEL, borderRadius: 14, padding: 20, marginBottom: 20 }}>
      <Text style={{ color: INK, fontSize: 18, lineHeight: 24 }}>{title}</Text>
      <Text style={{ color: MUTED, fontSize: 13, lineHeight: 19, marginBottom: 14 }}>{detail}</Text>
      {children}
    </View>
  );
}

module.exports = { SvgCardFrame };
