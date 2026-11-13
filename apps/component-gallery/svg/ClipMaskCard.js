const React = require('react');
const Svg = require('react-native-svg').default;
const {
  Defs,
  ClipPath,
  Mask,
  LinearGradient,
  Stop,
  Rect,
  Circle,
  Path,
  G,
  Use,
} = require('react-native-svg');
const { SvgCardFrame } = require('./SvgCardFrame');

// ClipPath, a luminance Mask with a gradient, and Use of a Defs symbol.
function ClipMaskCard() {
  return (
    <SvgCardFrame
      title="Clip, mask and use"
      detail="ClipPath (circle and star), Mask with a gradient (luminance), Use of a shape defined once in Defs"
    >
      <Svg width="100%" height={150} viewBox="0 0 560 150">
        <Defs>
          <ClipPath id="disc">
            <Circle cx="75" cy="75" r="60" />
          </ClipPath>
          <ClipPath id="star">
            <Path d="M215 15 L233 60 L282 62 L244 92 L257 140 L215 113 L173 140 L186 92 L148 62 L197 60 Z" />
          </ClipPath>
          <LinearGradient id="reveal" x1="0" y1="0" x2="1" y2="0">
            <Stop offset="0" stopColor="#000000" />
            <Stop offset="1" stopColor="#ffffff" />
          </LinearGradient>
          <Mask id="wipe" x="300" y="10" width="140" height="130" maskUnits="userSpaceOnUse">
            <Rect x="300" y="10" width="140" height="130" fill="url(#reveal)" />
          </Mask>
          <G id="badge">
            <Circle cx="0" cy="0" r="16" fill="#eb5757" />
            <Rect x="-8" y="-3" width="16" height="6" fill="#ffffff" />
          </G>
        </Defs>
        <Rect x="0" y="0" width="150" height="150" fill="#2f80ed" clipPath="url(#disc)" />
        <Rect
          x="0"
          y="0"
          width="150"
          height="150"
          fill="#f2c94c"
          clipPath="url(#disc)"
          transform="translate(40 40) scale(0.4)"
        />
        <Rect x="140" y="0" width="150" height="150" fill="#bb6bd9" clipPath="url(#star)" />
        <Rect x="300" y="10" width="140" height="130" fill="#79e8dc" mask="url(#wipe)" />
        <Use href="#badge" x="480" y="40" />
        <Use href="#badge" x="520" y="90" />
      </Svg>
    </SvgCardFrame>
  );
}

module.exports = { id: 'svg-clip-mask', title: 'Clip, mask and use', Component: ClipMaskCard };
