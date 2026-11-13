// Reanimated v4 on the QNX host: shared values, animated styles, withSpring,
// withTiming, withRepeat, derived values and a UI-thread frame callback.
//
// Drag-driven examples live in SpringCard.js and SwipeRow.js and need Gesture
// Handler; this card needs only Reanimated, so a failure here is not a gesture
// problem.
const React = require('react');
const { View, Text, Pressable } = require('react-native');
const Animated = require('react-native-reanimated').default;
const {
  useSharedValue,
  useAnimatedStyle,
  useDerivedValue,
  useFrameCallback,
  withSpring,
  withTiming,
  withRepeat,
  withSequence,
  withDelay,
  Easing,
  interpolate,
  interpolateColor,
  Extrapolation,
} = require('react-native-reanimated');
const { scheduleOnRN } = require('react-native-worklets');

const h = React.createElement;
const INK = '#e9f0f7';
const MUTED = '#9dafc2';
const ACCENT = '#79e8dc';
const PANEL = '#142538';

function log(line) {
  if (typeof globalThis.__rnqnxLog === 'function') globalThis.__rnqnxLog(line);
}

function Chip({ label, onPress }) {
  return h(
    Pressable,
    {
      onPress,
      accessibilityLabel: label,
      style: {
        backgroundColor: '#1b5b61',
        borderRadius: 8,
        padding: 10,
        marginRight: 8,
        marginBottom: 6,
      },
    },
    h(Text, { style: { color: INK } }, label)
  );
}

function ReanimatedCard() {
  const x = useSharedValue(0);
  const spin = useSharedValue(0);
  const pulse = useSharedValue(0);
  const frames = useSharedValue(0);
  const [frameCount, setFrameCount] = React.useState(0);
  const [done, setDone] = React.useState('idle');

  useFrameCallback(() => {
    'worklet';
    frames.value += 1;
  });

  const progress = useDerivedValue(() =>
    interpolate(x.value, [0, 220], [0, 1], Extrapolation.CLAMP)
  );

  const boxStyle = useAnimatedStyle(() => ({
    transform: [
      { translateX: x.value },
      { rotate: spin.value * 360 + 'deg' },
      { scale: 1 + pulse.value * 0.25 },
    ],
    opacity: 0.5 + progress.value * 0.5,
    backgroundColor: interpolateColor(progress.value, [0, 1], ['#79e8dc', '#f2b84b']),
  }));
  const barStyle = useAnimatedStyle(() => ({ width: 8 + progress.value * 220 }));

  const spring = () => {
    setDone('spring running');
    x.value = withSpring(x.value > 100 ? 0 : 220, { damping: 12, stiffness: 140 }, (finished) => {
      'worklet';
      scheduleOnRN(setDone, finished ? 'spring finished' : 'spring interrupted');
    });
  };
  const timing = () => {
    setDone('timing running');
    x.value = withTiming(
      x.value > 100 ? 0 : 220,
      { duration: 500, easing: Easing.out(Easing.cubic) },
      (finished) => {
        'worklet';
        scheduleOnRN(setDone, finished ? 'timing finished' : 'timing interrupted');
      }
    );
  };
  const loop = () => {
    spin.value = withRepeat(
      withTiming(1, { duration: 900, easing: Easing.linear }),
      2,
      false,
      () => {
        'worklet';
        spin.value = 0;
      }
    );
    pulse.value = withSequence(
      withTiming(1, { duration: 200 }),
      withDelay(100, withTiming(0, { duration: 300 }))
    );
  };
  const readFrames = () => {
    log('reanimated UI frames: ' + frames.value);
    setFrameCount(frames.value);
  };

  return h(
    View,
    { style: { backgroundColor: PANEL, borderRadius: 12, padding: 16, marginTop: 16 } },
    h(
      Text,
      { style: { color: INK, fontSize: 16, marginBottom: 6 } },
      'react-native-reanimated 4.7'
    ),
    h(
      View,
      { style: { flexDirection: 'row', flexWrap: 'wrap' } },
      h(Chip, { label: 'withSpring', onPress: spring }),
      h(Chip, { label: 'withTiming', onPress: timing }),
      h(Chip, { label: 'repeat + sequence', onPress: loop }),
      h(Chip, { label: 'read UI frames', onPress: readFrames })
    ),
    h(
      View,
      { style: { height: 96, justifyContent: 'center' } },
      h(Animated.View, {
        style: [
          { width: 56, height: 56, borderRadius: 12, position: 'absolute', left: 8 },
          boxStyle,
        ],
      })
    ),
    h(Animated.View, {
      style: [{ height: 6, borderRadius: 3, backgroundColor: ACCENT }, barStyle],
    }),
    h(
      Text,
      { style: { color: ACCENT, fontSize: 12, marginTop: 8 } },
      done + ', UI frame callback count ' + frameCount
    )
  );
}

module.exports = { ReanimatedCard };
