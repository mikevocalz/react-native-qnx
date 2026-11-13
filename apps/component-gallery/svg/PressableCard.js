const React = require('react');
const { View, Text, Pressable, Animated, Easing } = require('react-native');
const Svg = require('react-native-svg').default;
const { Circle, Rect, G, Path } = require('react-native-svg');
const { SvgCardFrame } = require('./SvgCardFrame');
const { INK, MUTED, SELECTED } = require('./theme');

const AnimatedCircle = Animated.createAnimatedComponent(Circle);
const COLORS = ['#2f80ed', '#6fcf97', '#f2994a', '#bb6bd9'];

// Per-element onPress hit testing, prop updates without a remount, and
// JS-driven animation (a timer re-renders with new props; the native element
// tree is retained and only repainted).
function PressableCard() {
  const [presses, setPresses] = React.useState(0);
  const [pressed, setPressed] = React.useState(false);
  const [groupTaps, setGroupTaps] = React.useState(0);
  const [angle, setAngle] = React.useState(0);
  const [spinning, setSpinning] = React.useState(true);
  // React Native Animated driving an SVG prop (r) from the JS thread.
  const pulse = React.useRef(new Animated.Value(0)).current;
  React.useEffect(() => {
    const loop = Animated.loop(
      Animated.sequence([
        Animated.timing(pulse, { toValue: 1, duration: 900, easing: Easing.inOut(Easing.ease), useNativeDriver: false }),
        Animated.timing(pulse, { toValue: 0, duration: 900, easing: Easing.inOut(Easing.ease), useNativeDriver: false }),
      ])
    );
    loop.start();
    return () => loop.stop();
  }, [pulse]);
  const pulseRadius = pulse.interpolate({ inputRange: [0, 1], outputRange: [6, 16] });

  React.useEffect(() => {
    if (!spinning) return undefined;
    const timer = setInterval(() => setAngle((a) => (a + 6) % 360), 50);
    return () => clearInterval(timer);
  }, [spinning]);

  return (
    <SvgCardFrame
      title="Touch targets and updates"
      detail="onPress on a Circle and on a G, press feedback via props, a JS-driven rotation and an Animated.Value radius (no Reanimated)"
    >
      <View style={{ flexDirection: 'row', alignItems: 'center', flexWrap: 'wrap' }}>
        <Svg width={300} height={150} viewBox="0 0 300 150">
          <Circle
            cx="75"
            cy="75"
            r={pressed ? 58 : 52}
            fill={COLORS[presses % COLORS.length]}
            stroke="#ffffff"
            strokeWidth={pressed ? 6 : 2}
            onPress={() => setPresses((n) => n + 1)}
            onPressIn={() => setPressed(true)}
            onPressOut={() => setPressed(false)}
          />
          <G
            onPress={() => setGroupTaps((n) => n + 1)}
            transform={`translate(225 75) rotate(${angle})`}
          >
            <Rect
              x="-40"
              y="-40"
              width="80"
              height="80"
              rx="10"
              fill="#142538"
              stroke="#79e8dc"
              strokeWidth={3}
            />
            <Path
              d="M-24 0 H24 M0 -24 V24"
              stroke="#79e8dc"
              strokeWidth={6}
              strokeLinecap="round"
            />
          </G>
        </Svg>
        <View style={{ marginLeft: 24 }}>
          <Text
            style={{ color: INK, fontSize: 15, lineHeight: 22 }}
          >{`Circle presses: ${presses}`}</Text>
          <Text
            style={{ color: INK, fontSize: 15, lineHeight: 22 }}
          >{`Group presses: ${groupTaps}`}</Text>
          <Text
            style={{ color: MUTED, fontSize: 13, lineHeight: 20 }}
          >{`Rotation: ${angle} deg`}</Text>
          <Pressable
            onPress={() => setSpinning((s) => !s)}
            accessibilityRole="button"
            accessibilityLabel="Toggle rotation"
            style={{
              marginTop: 10,
              minHeight: 36,
              paddingHorizontal: 16,
              justifyContent: 'center',
              backgroundColor: SELECTED,
              borderRadius: 9,
            }}
          >
            <Text style={{ color: '#f2ffff', fontSize: 14 }}>
              {spinning ? 'Pause rotation' : 'Resume rotation'}
            </Text>
          </Pressable>
        </View>
      </View>
    </SvgCardFrame>
  );
}

module.exports = { id: 'svg-touch', title: 'Touch targets and updates', Component: PressableCard };
