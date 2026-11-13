const React = require('react');
const Svg = require('react-native-svg').default;
const {
  Defs,
  ClipPath,
  LinearGradient,
  RadialGradient,
  Stop,
  Rect,
  Path,
  Line,
  Polygon,
  Circle,
} = require('react-native-svg');
const { SvgCardFrame } = require('./SvgCardFrame');

const STAR = 'M60 8 L72 100 L8 40 L112 40 L48 100 Z';

// fillRule and clipRule evenodd, stroke dashes and offsets, caps, joins,
// miter limit, non-scaling stroke under a viewBox, gradient spread methods.
function StrokesCard() {
  return (
    <SvgCardFrame
      title="Fill rules, strokes and gradient spread"
      detail="fillRule and clipRule evenodd, strokeDasharray and offset, linecap, linejoin, miterlimit, vectorEffect, spreadMethod"
    >
      <Svg width="100%" height={210} viewBox="0 0 560 210">
        <Defs>
          <ClipPath id="starClip">
            <Path d={STAR} />
          </ClipPath>
          <LinearGradient id="reflect" x1="0.3" y1="0" x2="0.7" y2="0" spreadMethod="reflect">
            <Stop offset="0" stopColor="#eb5757" />
            <Stop offset="1" stopColor="#2f80ed" />
          </LinearGradient>
          <LinearGradient id="repeat" x1="0.3" y1="0" x2="0.7" y2="0" spreadMethod="repeat">
            <Stop offset="0" stopColor="#eb5757" />
            <Stop offset="1" stopColor="#2f80ed" />
          </LinearGradient>
          <RadialGradient id="rad" cx="0.5" cy="0.5" r="0.25" spreadMethod="reflect">
            <Stop offset="0" stopColor="#f2c94c" />
            <Stop offset="1" stopColor="#bb6bd9" />
          </RadialGradient>
        </Defs>
        <Path d={STAR} fill="#79e8dc" fillRule="nonzero" transform="translate(0 0)" />
        <Path d={STAR} fill="#79e8dc" fillRule="evenodd" transform="translate(125 0)" />
        <Rect
          x="260"
          y="0"
          width="130"
          height="110"
          fill="#f2c94c"
          clipPath="url(#starClip)"
          clipRule="evenodd"
          transform="translate(-5 0)"
        />
        <Rect x="400" y="0" width="150" height="30" fill="url(#reflect)" />
        <Rect x="400" y="40" width="150" height="30" fill="url(#repeat)" />
        <Rect x="400" y="80" width="150" height="30" fill="url(#rad)" />

        <Line
          x1="10"
          y1="130"
          x2="260"
          y2="130"
          stroke="#e9f0f7"
          strokeWidth="6"
          strokeDasharray="14 8"
        />
        <Line
          x1="10"
          y1="148"
          x2="260"
          y2="148"
          stroke="#e9f0f7"
          strokeWidth="6"
          strokeDasharray="14 8"
          strokeDashoffset="11"
          strokeLinecap="round"
        />
        <Line
          x1="10"
          y1="166"
          x2="260"
          y2="166"
          stroke="#e9f0f7"
          strokeWidth="6"
          strokeDasharray="1 12"
          strokeLinecap="square"
        />
        <Path
          d="M290 190 L320 130 L350 190"
          fill="none"
          stroke="#6fcf97"
          strokeWidth="12"
          strokeLinejoin="miter"
          strokeMiterlimit="10"
        />
        <Path
          d="M370 190 L400 130 L430 190"
          fill="none"
          stroke="#6fcf97"
          strokeWidth="12"
          strokeLinejoin="miter"
          strokeMiterlimit="1.2"
        />
        <Path
          d="M450 190 L480 130 L510 190"
          fill="none"
          stroke="#6fcf97"
          strokeWidth="12"
          strokeLinejoin="round"
        />
        <Polygon points="530,130 550,130 540,150" fill="#eb5757" />
        <Circle
          cx="540"
          cy="180"
          r="14"
          fill="none"
          stroke="#e9f0f7"
          strokeWidth="3"
          vectorEffect="non-scaling-stroke"
        />
      </Svg>
    </SvgCardFrame>
  );
}

module.exports = {
  id: 'svg-strokes',
  title: 'Fill rules, strokes and gradient spread',
  Component: StrokesCard,
};
