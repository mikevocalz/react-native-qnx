const React = require('react');
const Svg = require('react-native-svg').default;
const {
  Defs,
  Filter,
  FeGaussianBlur,
  FeOffset,
  FeMerge,
  FeMergeNode,
  FeColorMatrix,
  FeFlood,
  FeComposite,
  FeBlend,
  FeMorphology,
  FeTurbulence,
  FeDisplacementMap,
  FeComponentTransfer,
  FeFuncR,
  FeFuncG,
  FeFuncB,
  FeDropShadow,
  FeConvolveMatrix,
  FeTile,
  FeDiffuseLighting,
  FeSpecularLighting,
  FeDistantLight,
  FePointLight,
  Rect,
  Circle,
  Path,
} = require('react-native-svg');
const { SvgCardFrame } = require('./SvgCardFrame');

// Every filter primitive the renderer implements, one shape each.
function FiltersCard() {
  return (
    <SvgCardFrame
      title="Filters"
      detail="feGaussianBlur, feOffset, feMerge, feColorMatrix, feFlood, feComposite, feBlend, feMorphology, feTurbulence, feDisplacementMap, feComponentTransfer, feDropShadow, feConvolveMatrix, feTile, lighting"
    >
      <Svg width="100%" height={330} viewBox="0 0 560 330">
        <Defs>
          <Filter id="blur">
            <FeGaussianBlur stdDeviation="4" />
          </Filter>
          <Filter id="shadow" x="-20%" y="-20%" width="150%" height="150%">
            <FeGaussianBlur in="SourceAlpha" stdDeviation="4" result="b" />
            <FeOffset in="b" dx="5" dy="6" result="o" />
            <FeMerge>
              <FeMergeNode in="o" />
              <FeMergeNode in="SourceGraphic" />
            </FeMerge>
          </Filter>
          <Filter id="dropShadow" x="-20%" y="-20%" width="150%" height="150%">
            <FeDropShadow dx="4" dy="4" stdDeviation="3" floodColor="#000000" floodOpacity="0.6" />
          </Filter>
          <Filter id="gray">
            <FeColorMatrix type="saturate" values="0" />
          </Filter>
          <Filter id="hue">
            <FeColorMatrix type="hueRotate" values="120" />
          </Filter>
          <Filter id="tint">
            <FeFlood floodColor="#eb5757" floodOpacity="0.7" result="f" />
            <FeComposite in="f" in2="SourceGraphic" operator="in" />
          </Filter>
          <Filter id="multiply">
            <FeFlood floodColor="#2f80ed" result="f" />
            <FeBlend in="SourceGraphic" in2="f" mode="multiply" />
          </Filter>
          <Filter id="dilate">
            <FeMorphology operator="dilate" radius="3" />
          </Filter>
          <Filter id="erode">
            <FeMorphology operator="erode" radius="3" />
          </Filter>
          <Filter id="noise" x="0" y="0" width="100%" height="100%">
            <FeTurbulence type="fractalNoise" baseFrequency="0.05" numOctaves="3" seed="2" />
          </Filter>
          <Filter id="warp">
            <FeTurbulence type="turbulence" baseFrequency="0.04" numOctaves="2" result="t" />
            <FeDisplacementMap in="SourceGraphic" in2="t" scale="14" xChannelSelector="R" yChannelSelector="G" />
          </Filter>
          <Filter id="transfer">
            <FeComponentTransfer>
              <FeFuncR type="linear" slope="0.5" intercept="0.4" />
              <FeFuncG type="discrete" tableValues="0 0.5 1" />
              <FeFuncB type="gamma" amplitude="1" exponent="2" offset="0" />
            </FeComponentTransfer>
          </Filter>
          <Filter id="sharpen">
            <FeConvolveMatrix order="3" kernelMatrix="0 -1 0 -1 5 -1 0 -1 0" />
          </Filter>
          <Filter id="tile" x="0" y="0" width="100%" height="100%">
            <FeOffset dx="10" dy="10" />
            <FeTile />
          </Filter>
          <Filter id="diffuse">
            <FeDiffuseLighting surfaceScale="5" diffuseConstant="1" lightingColor="#ffffff">
              <FeDistantLight azimuth="45" elevation="50" />
            </FeDiffuseLighting>
          </Filter>
          <Filter id="specular">
            <FeSpecularLighting surfaceScale="5" specularConstant="1" specularExponent="20" lightingColor="#ffffe0">
              <FePointLight x="40" y="20" z="60" />
            </FeSpecularLighting>
          </Filter>
        </Defs>
        <Circle cx="35" cy="35" r="24" fill="#f2c94c" filter="url(#blur)" />
        <Rect x="80" y="12" width="50" height="46" fill="#6fcf97" filter="url(#shadow)" />
        <Rect x="150" y="12" width="50" height="46" fill="#bb6bd9" filter="url(#dropShadow)" />
        <Rect x="220" y="12" width="50" height="46" fill="#eb5757" filter="url(#gray)" />
        <Rect x="290" y="12" width="50" height="46" fill="#eb5757" filter="url(#hue)" />
        <Circle cx="385" cy="35" r="24" fill="#f2c94c" filter="url(#tint)" />
        <Circle cx="455" cy="35" r="24" fill="#f2c94c" filter="url(#multiply)" />
        <Path d="M500 12 h46 v46 h-46 z M510 22 h26 v26 h-26 z" fill="#e9f0f7" fillRule="evenodd" filter="url(#dilate)" />

        <Path d="M10 90 h56 v50 h-56 z M20 100 h36 v30 h-36 z" fill="#e9f0f7" fillRule="evenodd" filter="url(#erode)" />
        <Rect x="80" y="82" width="70" height="60" filter="url(#noise)" />
        <Rect x="170" y="82" width="70" height="60" fill="#56ccf2" filter="url(#warp)" />
        <Rect x="260" y="82" width="70" height="60" fill="#79e8dc" filter="url(#transfer)" />
        <Circle cx="385" cy="112" r="28" fill="#6fcf97" stroke="#142538" strokeWidth="6" filter="url(#sharpen)" />
        <Rect x="440" y="82" width="110" height="60" fill="#f2994a" filter="url(#tile)" />

        <Rect x="10" y="170" width="120" height="70" rx="12" fill="#f2c94c" filter="url(#diffuse)" />
        <Circle cx="215" cy="205" r="34" fill="#bb6bd9" filter="url(#specular)" />
      </Svg>
    </SvgCardFrame>
  );
}

module.exports = { id: 'svg-filters', title: 'Filters', Component: FiltersCard };
