const React = require('react');
const { View } = require('react-native');
const { SvgXml, SvgAst, parse } = require('react-native-svg');
const { SvgCss } = require('react-native-svg/css');
const { SvgCardFrame } = require('./SvgCardFrame');

const XML = `<svg width="120" height="80" viewBox="0 0 120 80" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <linearGradient id="g" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="#79e8dc"/><stop offset="1" stop-color="#2f80ed"/>
    </linearGradient>
  </defs>
  <rect x="4" y="4" width="112" height="72" rx="10" fill="url(#g)"/>
  <circle cx="40" cy="40" r="18" fill="#f2c94c" stroke="#142538" stroke-width="3"/>
  <path d="M70 55 L85 25 L100 55 Z" fill="#eb5757" fill-rule="evenodd"/>
</svg>`;

const CSS = `<svg width="120" height="80" viewBox="0 0 120 80" xmlns="http://www.w3.org/2000/svg">
  <style>
    .a { fill: #bb6bd9; }
    #b { fill: #6fcf97; stroke: #142538; stroke-width: 2; }
    g > circle { opacity: 0.8; }
  </style>
  <g><circle class="a" cx="35" cy="40" r="26"/><circle id="b" cx="75" cy="40" r="26"/></g>
</svg>`;

// SvgXml and SvgCss (react-native-svg's utility exports) parse markup into the
// same elements an app writes by hand, so they take the same renderer path.
function XmlCard() {
  return (
    <SvgCardFrame
      title="SvgXml, SvgCss and parse"
      detail="Markup strings through react-native-svg's own parser: gradient, stroke, fill-rule, <style> rules and selectors"
    >
      <View style={{ flexDirection: 'row', alignItems: 'center' }}>
        <SvgXml xml={XML} width={240} height={160} />
        <SvgCss xml={CSS} width={240} height={160} />
        <SvgAst ast={parse(XML)} override={{ width: 120, height: 80 }} />
      </View>
    </SvgCardFrame>
  );
}

module.exports = { id: 'svg-xml', title: 'SvgXml, SvgCss and parse', Component: XmlCard };
