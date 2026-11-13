const React = require('react');
const Svg = require('react-native-svg').default;
const {
  Defs,
  LinearGradient,
  RadialGradient,
  Stop,
  Rect,
  Circle,
  Path,
} = require('react-native-svg');
const { SvgCardFrame } = require('./SvgCardFrame');

// Gradient servers in <Defs>, referenced with url(#id): bounding-box and
// user-space units, an elliptical radial gradient with an off-center focus,
// and stops with partial alpha.
function GradientsCard() {
  return (
    <SvgCardFrame
      title="Gradients"
      detail="LinearGradient and RadialGradient in Defs, objectBoundingBox and userSpaceOnUse, stopOpacity, fill and stroke"
    >
      <Svg width="100%" height={150} viewBox="0 0 560 150">
        <Defs>
          <LinearGradient id="sunset" x1="0" y1="0" x2="1" y2="1">
            <Stop offset="0" stopColor="#f2994a" />
            <Stop offset="0.5" stopColor="#eb5757" />
            <Stop offset="1" stopColor="#6a1b9a" />
          </LinearGradient>
          <RadialGradient id="orb" cx="0.5" cy="0.5" rx="0.5" ry="0.5" fx="0.35" fy="0.3">
            <Stop offset="0" stopColor="#ffffff" />
            <Stop offset="0.45" stopColor="#56ccf2" />
            <Stop offset="1" stopColor="#1b3a6b" />
          </RadialGradient>
          <LinearGradient id="fade" x1="0" y1="0" x2="1" y2="0">
            <Stop offset="0" stopColor="#79e8dc" stopOpacity="1" />
            <Stop offset="1" stopColor="#79e8dc" stopOpacity="0" />
          </LinearGradient>
          <LinearGradient id="bar" gradientUnits="userSpaceOnUse" x1="300" y1="0" x2="520" y2="0">
            <Stop offset="0" stopColor="#6fcf97" />
            <Stop offset="1" stopColor="#2f80ed" />
          </LinearGradient>
          <RadialGradient id="ellipse" cx="0.5" cy="0.5" rx="0.5" ry="0.3">
            <Stop offset="0" stopColor="#f2c94c" />
            <Stop offset="1" stopColor="#f2c94c" stopOpacity="0" />
          </RadialGradient>
        </Defs>
        <Rect x="10" y="10" width="130" height="130" rx="12" fill="url(#sunset)" />
        <Circle cx="215" cy="75" r="65" fill="url(#orb)" />
        <Rect x="300" y="10" width="250" height="30" rx="6" fill="url(#bar)" />
        <Rect
          x="300"
          y="55"
          width="250"
          height="30"
          rx="6"
          fill="url(#fade)"
          stroke="#79e8dc"
          strokeWidth={2}
        />
        <Path d="M300 140 L 550 140 L 550 100 Q 425 80 300 100 Z" fill="url(#ellipse)" />
        <Circle cx="425" cy="120" r="14" fill="none" stroke="url(#bar)" strokeWidth={6} />
      </Svg>
    </SvgCardFrame>
  );
}

module.exports = { id: 'svg-gradients', title: 'Gradients', Component: GradientsCard };
