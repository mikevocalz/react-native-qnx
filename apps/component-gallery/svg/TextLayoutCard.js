const React = require('react');
const Svg = require('react-native-svg').default;
const { Defs, Path, Text, TSpan, TextPath } = require('react-native-svg');
const { SvgCardFrame } = require('./SvgCardFrame');

// Per-character x, y, dx, dy and rotate lists, one text-anchor across several
// spans, textLength with both lengthAdjust modes, letter and word spacing,
// decorations, baseline shifts and text on a path.
function TextLayoutCard() {
  return (
    <SvgCardFrame
      title="Text layout"
      detail="x/y/dx/dy/rotate lists, anchor across spans, textLength, letterSpacing, wordSpacing, textDecoration, baselineShift, TextPath"
    >
      <Svg width="100%" height={260} viewBox="0 0 560 260">
        <Defs>
          <Path id="wave" d="M20 230 C 90 160, 160 300, 230 230 S 370 160, 440 230" />
        </Defs>
        <Text x="20 40 60 80 100 120" y="30 24 36 24 36 30" fill="#e9f0f7" fontSize="26">
          Positions
        </Text>
        <Text x="220" y="34" rotate="0 10 20 30 40 50 60 70" fill="#79e8dc" fontSize="26">
          Rotated
        </Text>
        <Text x="380" y="34" fill="#f2c94c" fontSize="22" dx="0 6 12 18" dy="0 -4 4 -4">
          Shifted
        </Text>
        <Text x="280" y="76" textAnchor="middle" fill="#e9f0f7" fontSize="22">
          <TSpan>One anchor</TSpan>
          <TSpan fill="#eb5757" fontSize="30">
            {' '}
            across
          </TSpan>
          <TSpan fill="#6fcf97"> spans</TSpan>
        </Text>
        <Text x="20" y="116" textLength="260" lengthAdjust="spacing" fill="#e9f0f7" fontSize="22">
          Spacing to 260
        </Text>
        <Text
          x="300"
          y="116"
          textLength="240"
          lengthAdjust="spacingAndGlyphs"
          fill="#e9f0f7"
          fontSize="22"
        >
          Glyphs to 240
        </Text>
        <Text x="20" y="152" letterSpacing="5" fill="#bb6bd9" fontSize="20">
          Letter spacing
        </Text>
        <Text x="250" y="152" wordSpacing="22" fill="#56ccf2" fontSize="20">
          Word spacing here
        </Text>
        <Text x="20" y="188" textDecoration="underline" fill="#e9f0f7" fontSize="20">
          Underline
        </Text>
        <Text x="150" y="188" textDecoration="line-through" fill="#e9f0f7" fontSize="20">
          Strike
        </Text>
        <Text x="250" y="188" textDecoration="overline" fill="#e9f0f7" fontSize="20">
          Overline
        </Text>
        <Text x="350" y="188" fill="#e9f0f7" fontSize="20">
          x<TSpan baselineShift="super" fontSize="13">2</TSpan>
          {' + y'}
          <TSpan baselineShift="sub" fontSize="13">i</TSpan>
        </Text>
        <Text fill="#f2994a" fontSize="22">
          <TextPath href="#wave" startOffset="5%">
            Text along a wave
          </TextPath>
        </Text>
      </Svg>
    </SvgCardFrame>
  );
}

module.exports = { id: 'svg-text-layout', title: 'Text layout', Component: TextLayoutCard };
