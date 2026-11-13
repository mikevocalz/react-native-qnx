// Real react-native-svg 15.16.0 elements, mounted through the production
// Fabric -> Qt path, for tests/native/qt-rnsvg-fabric.
const React = require('react');
const { useState } = React;
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const Svg = require('react-native-svg').default;
const {
  Defs,
  LinearGradient,
  RadialGradient,
  Stop,
  ClipPath,
  Mask,
  G,
  Rect,
  Circle,
  Ellipse,
  Line,
  Path,
  Polygon,
  Polyline,
  Text,
  TSpan,
  Use,
  Image,
} = require('react-native-svg');

globalThis.__rnsvgPresses = 0;
globalThis.__rnsvgGroupPresses = 0;
globalThis.__rnsvgMounts = 0;

function Fixture() {
  const [state, setState] = useState({
    pathFill: '#00ff00',
    gradientEnd: '#0000ff',
    showCircle: true,
    width: 200,
  });
  globalThis.__rnsvgUpdate = (patch) => setState((previous) => ({ ...previous, ...patch }));
  React.useEffect(() => {
    globalThis.__rnsvgMounts++;
  }, []);
  return (
    <Svg
      width={state.width}
      height={150}
      viewBox="0 0 200 150"
      style={{ position: 'absolute', left: 0, top: 0 }}
    >
      <Defs>
        <LinearGradient id="grad" x1="0" y1="0" x2="1" y2="0">
          <Stop offset="0" stopColor="#ff0000" />
          <Stop offset="1" stopColor={state.gradientEnd} />
        </LinearGradient>
        <RadialGradient id="rad" cx="0.5" cy="0.5" rx="0.5" ry="0.5">
          <Stop offset="0" stopColor="#ffffff" />
          <Stop offset="1" stopColor="#000000" />
        </RadialGradient>
        <ClipPath id="clip">
          <Circle cx="150" cy="100" r="30" />
        </ClipPath>
        <Mask id="mask" x="0" y="0" width="200" height="150" maskUnits="userSpaceOnUse">
          <Rect x="0" y="0" width="100" height="150" fill="#ffffff" />
        </Mask>
        <Rect id="box" x="0" y="0" width="20" height="20" fill="#ff8000" />
      </Defs>
      <Rect x="0" y="0" width="100" height="40" fill="url(#grad)" />
      <G fill="#00ff00">
        <Rect x="0" y="50" width="40" height="40" />
        <Rect x="50" y="50" width="40" height="40" fill="#ff0000" />
      </G>
      <Rect x="100" y="0" width="100" height="100" fill="#ff0000" clipPath="url(#clip)" />
      <Rect x="0" y="100" width="200" height="20" fill="#0000ff" mask="url(#mask)" />
      <Use href="#box" x="100" y="120" />
      {state.showCircle ? <Circle cx="170" cy="30" r="20" fill="#800080" /> : null}
      <G onPress={() => globalThis.__rnsvgGroupPresses++}>
        <Path
          d="M10 125 h60 v20 h-60z"
          fill={state.pathFill}
          onPress={() => globalThis.__rnsvgPresses++}
        />
      </G>
    </Svg>
  );
}

AppRegistry.registerComponent('RnqnxRnsvgFixture', () => Fixture);
