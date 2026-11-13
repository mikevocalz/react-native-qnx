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
  Pattern,
  // Not `Symbol`: that would shadow the global the Babel helpers rely on.
  Symbol: SvgSymbol,
  Marker,
  TextPath,
  ForeignObject,
} = require('react-native-svg');
const View = require('react-native/Libraries/Components/View/View').default;

globalThis.__rnsvgPresses = 0;
globalThis.__rnsvgGroupPresses = 0;
globalThis.__rnsvgMounts = 0;
globalThis.__rnsvgForeignTouches = 0;
// onSvgLayout of the extras <Rect>: how many events, and the last rect.
globalThis.__rnsvgLayoutEvents = 0;
globalThis.__rnsvgLayoutX = -1;
globalThis.__rnsvgLayoutY = -1;
globalThis.__rnsvgLayoutW = -1;
globalThis.__rnsvgLayoutH = -1;

function Fixture() {
  const [state, setState] = useState({
    pathFill: '#00ff00',
    gradientEnd: '#0000ff',
    showCircle: true,
    width: 200,
    extras: false,
    rectX: 10,
    rectFill: '#ff00ff',
  });
  globalThis.__rnsvgUpdate = (patch) => setState((previous) => ({ ...previous, ...patch }));
  // Counted in render: passive effects wait on a scheduler tick this host
  // does not guarantee, and the test waits on the mounted tree anyway.
  globalThis.__rnsvgMounts++;
  const main = (
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
  // A second <Svg> over the first, mounted on demand: the elements added in
  // react-native-svg 15.16.0 beyond the first fixture, with the props the
  // bridge maps for them. The first <Svg> keeps its place when this appears.
  const extras = (
    <Svg
      width={200}
      height={150}
      viewBox="0 0 200 150"
      style={{ position: 'absolute', left: 0, top: 0 }}
    >
      <Defs>
        <Pattern
          id="pat"
          x="2"
          y="3"
          width="20"
          height="30%"
          patternUnits="userSpaceOnUse"
          patternContentUnits="objectBoundingBox"
          viewBox="0 0 10 10"
          preserveAspectRatio="xMinYMax slice"
          patternTransform="rotate(10)"
        >
          <Rect x="0" y="0" width="5" height="5" fill="#ff0000" />
        </Pattern>
        <SvgSymbol id="sym" viewBox="0 0 24 24" preserveAspectRatio="none">
          <Circle cx="12" cy="12" r="10" fill="#00ff00" />
        </SvgSymbol>
        <Marker
          id="mk"
          markerWidth="6"
          markerHeight="7"
          refX="3"
          refY="4"
          orient="auto-start-reverse"
          markerUnits="userSpaceOnUse"
          viewBox="0 0 10 10"
        >
          <Path d="M0 0 L10 5 L0 10z" fill="#0000ff" />
        </Marker>
        <Path id="curve" d="M10 90 C 60 40 120 140 190 90" />
      </Defs>
      <Rect x="0" y="0" width="40" height="40" fill="url(#pat)" />
      <Use href="#sym" x="50" y="0" width="24" height="24" />
      <Path
        d="M10 120 L60 120 L90 110"
        stroke="#000000"
        fill="none"
        markerStart="url(#mk)"
        markerMid="url(#mk)"
        markerEnd="url(#mk)"
        clipRule="evenodd"
        filter="url(#absent)"
      />
      <Circle cx="150" cy="20" r="5" fill="#808080" clipRule="nonzero" />
      <Circle cx="170" cy="20" r="5" fill="#808080" />
      <Rect
        x={state.rectX}
        y="60"
        width="30"
        height="20"
        fill={state.rectFill}
        onLayout={(event) => {
          const layout = event.nativeEvent.layout;
          globalThis.__rnsvgLayoutEvents++;
          globalThis.__rnsvgLayoutX = layout.x;
          globalThis.__rnsvgLayoutY = layout.y;
          globalThis.__rnsvgLayoutW = layout.width;
          globalThis.__rnsvgLayoutH = layout.height;
        }}
      />
      <Text
        x={[10, 20]}
        y="100"
        dx="1"
        rotate={[5, 10, 15]}
        fontSize="14"
        fontWeight="bold"
        textAnchor="middle"
        inlineSize="80"
        baselineShift="sub"
        alignmentBaseline="middle"
        wordSpacing="2"
        kerning="1"
        letterSpacing="0.5"
        textDecoration="underline"
        fontVariantLigatures="none"
        fontStretch="condensed"
        fontFeatureSettings='"liga" 0'
      >
        Hello
        <TSpan x="30" y="100" dy="2" rotate="45" verticalAlign="-4 middle">
          World
        </TSpan>
      </Text>
      <Text>
        <TextPath
          href="#curve"
          startOffset="10"
          method="stretch"
          spacing="exact"
          midLine="sharp"
          side="right"
        >
          On a path
        </TextPath>
      </Text>
      <ForeignObject x="100" y="100" width="80" height="40">
        <View
          style={{ width: 30, height: 20, backgroundColor: '#ff0000' }}
          onTouchEnd={() => globalThis.__rnsvgForeignTouches++}
        />
      </ForeignObject>
    </Svg>
  );
  return (
    <>
      {main}
      {state.extras ? extras : null}
    </>
  );
}

AppRegistry.registerComponent('RnqnxRnsvgFixture', () => Fixture);
