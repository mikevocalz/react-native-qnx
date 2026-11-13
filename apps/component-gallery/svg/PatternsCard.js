const React = require('react');
const Svg = require('react-native-svg').default;
const { Defs, Pattern, Symbol, Marker, Use, Rect, Circle, Path, Line, G } = require('react-native-svg');
const { SvgCardFrame } = require('./SvgCardFrame');

// Pattern in both unit systems and with a viewBox and patternTransform, Symbol
// instantiated at three sizes through Use, and Marker on a path, a polyline
// and a line (orient auto, strokeWidth units, context-stroke fill).
function PatternsCard() {
  return (
    <SvgCardFrame
      title="Pattern, symbol and marker"
      detail="Pattern (userSpaceOnUse, objectBoundingBox, viewBox, patternTransform), Symbol with viewBox via Use, Marker with orient auto"
    >
      <Svg width="100%" height={170} viewBox="0 0 560 170">
        <Defs>
          <Pattern id="dots" x="0" y="0" width="20" height="20" patternUnits="userSpaceOnUse">
            <Circle cx="10" cy="10" r="6" fill="#f2c94c" />
          </Pattern>
          <Pattern id="tiles" x="0" y="0" width="0.25" height="0.25">
            <Rect x="0" y="0" width="20" height="20" fill="#6fcf97" />
            <Rect x="20" y="20" width="20" height="20" fill="#2f80ed" />
          </Pattern>
          <Pattern
            id="stripes"
            x="0"
            y="0"
            width="16"
            height="16"
            patternUnits="userSpaceOnUse"
            patternTransform="rotate(45)"
            viewBox="0 0 8 8"
          >
            <Rect x="0" y="0" width="4" height="8" fill="#bb6bd9" />
          </Pattern>
          <Symbol id="badge" viewBox="0 0 24 24">
            <Circle cx="12" cy="12" r="11" fill="#eb5757" />
            <Path d="M7 12l3.5 3.5L17 9" stroke="#ffffff" strokeWidth="3" fill="none" />
          </Symbol>
          <Marker
            id="arrow"
            viewBox="0 0 10 10"
            refX="5"
            refY="5"
            markerWidth="6"
            markerHeight="6"
            orient="auto"
          >
            <Path d="M0 0 L10 5 L0 10 z" fill="context-stroke" />
          </Marker>
          <Marker id="dot" viewBox="0 0 10 10" refX="5" refY="5" markerWidth="4" markerHeight="4">
            <Circle cx="5" cy="5" r="5" fill="#ffffff" />
          </Marker>
        </Defs>
        <Rect x="10" y="10" width="140" height="70" rx="8" fill="url(#dots)" stroke="#e9f0f7" />
        <Rect x="10" y="95" width="140" height="65" rx="8" fill="url(#tiles)" stroke="#e9f0f7" />
        <Circle cx="215" cy="85" r="68" fill="url(#stripes)" stroke="#e9f0f7" />
        <Use href="#badge" x="300" y="10" width="24" height="24" />
        <Use href="#badge" x="300" y="45" width="48" height="48" />
        <Use href="#badge" x="300" y="105" width="56" height="56" />
        <Path
          d="M380 150 C 410 20, 470 20, 500 90 S 540 150, 550 60"
          fill="none"
          stroke="#79e8dc"
          strokeWidth="4"
          markerStart="url(#dot)"
          markerMid="url(#dot)"
          markerEnd="url(#arrow)"
        />
        <Line
          x1="380"
          y1="20"
          x2="540"
          y2="20"
          stroke="#f2994a"
          strokeWidth="3"
          markerEnd="url(#arrow)"
        />
        <G />
      </Svg>
    </SvgCardFrame>
  );
}

module.exports = { id: 'svg-patterns', title: 'Pattern, symbol and marker', Component: PatternsCard };
