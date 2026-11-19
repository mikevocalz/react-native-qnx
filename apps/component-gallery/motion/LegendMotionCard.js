// @legendapp/motion on the QNX host.
//
// Motion drives React Native's core Animated (Animated.View/Value/timing/spring).
// This card exercises initial -> animate, a spring transition, whileTap,
// AnimatePresence exit, and a transform/opacity/color mix, so a failure points
// at the Animated driver or the renderer, not at the library.
const React = require('react');
const { View, Text, Pressable, Animated, Easing } = require('react-native');
const { Motion, AnimatePresence } = require('@legendapp/motion');

const h = React.createElement;
const INK = '#e9f0f7';
const MUTED = '#9dafc2';
const ACCENT = '#79e8dc';
const PANEL = '#142538';

function Chip({ label, onPress }) {
  return h(
    Pressable,
    {
      onPress,
      accessibilityLabel: label,
      style: { backgroundColor: '#1b5b61', borderRadius: 8, padding: 10, marginRight: 8 },
    },
    h(Text, { style: { color: INK } }, label)
  );
}

function LegendMotionCard() {
  const [on, setOn] = React.useState(false);
  const [shown, setShown] = React.useState(true);
  const [taps, setTaps] = React.useState(0);
  // Core Animated reference run next to Motion, so one can tell a driver
  // problem from a Motion problem.
  const ref = React.useRef(new Animated.Value(0)).current;
  React.useEffect(() => {
    Animated.timing(ref, {
      toValue: on ? 1 : 0,
      duration: 400,
      easing: Easing.out(Easing.cubic),
      useNativeDriver: true,
    }).start();
  }, [on, ref]);

  return h(
    View,
    { style: { backgroundColor: PANEL, borderRadius: 12, padding: 16, marginTop: 16 } },
    h(
      Text,
      { style: { color: INK, fontSize: 16, marginBottom: 6 } },
      '@legendapp/motion 2.5.3 (core Animated)'
    ),
    h(
      View,
      { style: { flexDirection: 'row', marginBottom: 12 } },
      h(Chip, { label: on ? 'Animate back' : 'Animate', onPress: () => setOn((value) => !value) }),
      h(Chip, {
        label: shown ? 'Hide (exit)' : 'Show (enter)',
        onPress: () => setShown((value) => !value),
      })
    ),
    h(
      View,
      { style: { height: 120, justifyContent: 'center' } },
      h(Motion.View, {
        initial: { x: 0, opacity: 0.4, scale: 1, rotate: '0deg' },
        animate: on
          ? { x: 220, opacity: 1, scale: 1.25, rotate: '45deg' }
          : { x: 0, opacity: 0.4, scale: 1, rotate: '0deg' },
        transition: { type: 'spring', damping: 12, stiffness: 160 },
        style: {
          width: 56,
          height: 56,
          borderRadius: 12,
          backgroundColor: ACCENT,
          position: 'absolute',
          left: 8,
        },
      }),
      h(Animated.View, {
        style: {
          width: 24,
          height: 24,
          borderRadius: 12,
          backgroundColor: '#f2b84b',
          position: 'absolute',
          left: 8,
          bottom: 4,
          opacity: ref,
          transform: [
            { translateX: ref.interpolate({ inputRange: [0, 1], outputRange: [0, 220] }) },
          ],
        },
      })
    ),
    h(
      AnimatePresence,
      null,
      shown
        ? h(
            Motion.View,
            {
              key: 'presence',
              initial: { y: 20, opacity: 0 },
              animate: { y: 0, opacity: 1 },
              exit: { y: -20, opacity: 0 },
              transition: { type: 'timing', duration: 300 },
              style: { backgroundColor: '#1d3a52', borderRadius: 8, padding: 10, marginTop: 8 },
            },
            h(Text, { style: { color: INK } }, 'AnimatePresence child (enter/exit)')
          )
        : null
    ),
    h(
      Motion.View,
      {
        whileTap: { scale: 0.92, opacity: 0.7 },
        transition: { type: 'spring', damping: 14, stiffness: 300 },
        onTouchEnd: () => setTaps((value) => value + 1),
        style: {
          backgroundColor: '#3b4b63',
          borderRadius: 8,
          padding: 12,
          marginTop: 12,
          alignSelf: 'flex-start',
        },
      },
      h(Text, { style: { color: INK } }, 'whileTap target, touches: ' + taps)
    ),
    h(
      Text,
      { style: { color: MUTED, fontSize: 12, marginTop: 8 } },
      'Gold dot: core Animated timing, native driver requested.'
    )
  );
}

module.exports = { LegendMotionCard };
