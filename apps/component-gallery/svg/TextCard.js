const React = require('react');
const Svg = require('react-native-svg').default;
const { Defs, LinearGradient, Stop, Text, TSpan } = require('react-native-svg');
const { SvgCardFrame } = require('./SvgCardFrame');

// Text and TSpan: positioning, textAnchor, font props, stroke and gradient fill.
function TextCard() {
  return (
    <SvgCardFrame
      title="Text"
      detail="Text and TSpan with x, y, dx, dy, textAnchor, fontSize, fontWeight, stroked and gradient-filled text"
    >
      <Svg width="100%" height={150} viewBox="0 0 560 150">
        <Defs>
          <LinearGradient id="ink" x1="0" y1="0" x2="1" y2="0">
            <Stop offset="0" stopColor="#79e8dc" />
            <Stop offset="1" stopColor="#bb6bd9" />
          </LinearGradient>
        </Defs>
        <Text x="20" y="40" fill="#e9f0f7" fontSize="30">
          Start anchored
        </Text>
        <Text x="280" y="82" fill="url(#ink)" fontSize="38" fontWeight="bold" textAnchor="middle">
          Gradient text
        </Text>
        <Text x="540" y="40" fill="#f2c94c" fontSize="24" textAnchor="end">
          End anchored
        </Text>
        <Text
          x="20"
          y="130"
          fill="none"
          stroke="#79e8dc"
          strokeWidth={1.5}
          fontSize="40"
          fontWeight="bold"
        >
          Outline
        </Text>
        <Text x="250" y="130" fill="#e9f0f7" fontSize="22">
          Spans:
          <TSpan fill="#eb5757" fontWeight="bold">
            {' '}
            bold red
          </TSpan>
          <TSpan fill="#6fcf97" dy="-8">
            {' '}
            raised
          </TSpan>
          <TSpan fill="#56ccf2" dy="8">
            {' '}
            normal
          </TSpan>
        </Text>
      </Svg>
    </SvgCardFrame>
  );
}

module.exports = { id: 'svg-text', title: 'Text', Component: TextCard };
