const React = require('react');
const Svg = require('react-native-svg').default;
const { Path, Circle, Ellipse, Rect, Line, Polygon, Polyline, G } = require('react-native-svg');
const { SvgCardFrame } = require('./SvgCardFrame');

// Basic shapes, path commands, stroke styling, and fill/stroke inheritance
// from a <G>.
function PathsCard() {
  return (
    <SvgCardFrame
      title="Paths and shapes"
      detail="Path (M L C Q A Z), Rect, Circle, Ellipse, Line, Polygon, Polyline, dashes, caps, joins and evenodd"
    >
      <Svg width="100%" height={150} viewBox="0 0 560 150">
        <G stroke="#79e8dc" strokeWidth={3} fill="none">
          <Path d="M20 110 C 20 30, 110 30, 110 110 S 200 140, 200 110" />
          <Path d="M230 110 A 40 40 0 0 1 310 110 A 40 40 0 0 1 230 110 Z" fill="#1b5b61" />
          <Path
            d="M340 20 L 400 20 L 370 80 Z M 350 30 L 390 30 L 370 66 Z"
            fill="#f2c94c"
            fillRule="evenodd"
            stroke="none"
          />
          <Rect x="420" y="20" width="60" height="60" rx="14" fill="#eb5757" stroke="#ffffff" />
          <Circle cx="520" cy="50" r="24" fill="#2f80ed" />
        </G>
        <Ellipse cx="60" cy="135" rx="40" ry="10" fill="#bb6bd9" />
        <Line
          x1="120"
          y1="135"
          x2="260"
          y2="135"
          stroke="#ffffff"
          strokeWidth={8}
          strokeLinecap="round"
        />
        <Line
          x1="280"
          y1="135"
          x2="420"
          y2="135"
          stroke="#ffffff"
          strokeWidth={4}
          strokeDasharray="12 8"
        />
        <Polygon points="440,95 470,145 410,145" fill="#6fcf97" />
        <Polyline
          points="490,145 510,100 530,145 550,100"
          fill="none"
          stroke="#f2994a"
          strokeWidth={5}
          strokeLinejoin="round"
        />
      </Svg>
    </SvgCardFrame>
  );
}

module.exports = { id: 'svg-paths', title: 'Paths and shapes', Component: PathsCard };
