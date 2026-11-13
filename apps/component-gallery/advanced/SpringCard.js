// A card you drag with the mouse; it springs back on release.
//
// Gesture Handler 3 pan recognizes in native code (vnext/src/gesture); its
// worklet callbacks run on the UI-worklet thread and write shared values, which
// Reanimated applies to the mounted Qt item. The busy-JS button blocks the JS
// thread for 400 ms mid-gesture. On this host the JS thread is also the Qt GUI
// thread, so the screen cannot repaint during the block; the card catches up
// afterwards from UI-thread state (docs/reanimated-gesture-handler-qnx.md).
const React = require('react');
const { View, Text, Pressable } = require('react-native');
const Animated = require('react-native-reanimated').default;
const {
  useSharedValue,
  useAnimatedStyle,
  withSpring,
  withTiming,
} = require('react-native-reanimated');
const {
  GestureHandlerRootView,
  GestureDetector,
  usePanGesture,
  useTapGesture,
  useExclusiveGestures,
} = require('react-native-gesture-handler');
const { scheduleOnRN } = require('react-native-worklets');

const h = React.createElement;
const INK = '#e9f0f7';
const MUTED = '#9dafc2';
const ACCENT = '#79e8dc';
const PANEL = '#142538';

function log(line) {
  if (typeof globalThis.__rnqnxLog === 'function') globalThis.__rnqnxLog(line);
}

function SpringCard() {
  const x = useSharedValue(0);
  const y = useSharedValue(0);
  const startX = useSharedValue(0);
  const startY = useSharedValue(0);
  const lift = useSharedValue(0);
  const [status, setStatus] = React.useState('drag the card');
  const [taps, setTaps] = React.useState(0);

  const pan = usePanGesture({
    onBegin: () => {
      'worklet';
      startX.value = x.value;
      startY.value = y.value;
      lift.value = withTiming(1, { duration: 90 });
      scheduleOnRN(setStatus, 'pan began');
    },
    onUpdate: (event) => {
      'worklet';
      x.value = startX.value + event.translationX;
      y.value = startY.value + event.translationY;
    },
    onDeactivate: (event) => {
      'worklet';
      x.value = withSpring(0, { damping: 14, stiffness: 160, velocity: event.velocityX });
      y.value = withSpring(0, { damping: 14, stiffness: 160, velocity: event.velocityY });
      scheduleOnRN(
        setStatus,
        'released at ' +
          Math.round(event.translationX) +
          ', ' +
          Math.round(event.translationY) +
          ', springing back'
      );
    },
    onFinalize: () => {
      'worklet';
      lift.value = withTiming(0, { duration: 140 });
    },
  });

  const doubleTap = useTapGesture({
    numberOfTaps: 2,
    onDeactivate: () => {
      'worklet';
      scheduleOnRN(setStatus, 'double tap');
    },
  });
  const singleTap = useTapGesture({
    onDeactivate: () => {
      'worklet';
      scheduleOnRN(setTaps, (value) => value + 1);
    },
  });
  // Single tap waits for the double tap to fail; the pan competes with both.
  const taps_ = useExclusiveGestures(doubleTap, singleTap);

  const cardStyle = useAnimatedStyle(() => ({
    transform: [{ translateX: x.value }, { translateY: y.value }, { scale: 1 + lift.value * 0.06 }],
    opacity: 0.85 + lift.value * 0.15,
  }));

  const busy = () => {
    log('SpringCard: blocking JS for 400 ms');
    const end = Date.now() + 400;
    while (Date.now() < end) {
      // JS thread busy.
    }
    log('SpringCard: JS unblocked');
  };

  return h(
    View,
    { style: { backgroundColor: PANEL, borderRadius: 12, padding: 16, marginTop: 16 } },
    h(
      Text,
      { style: { color: INK, fontSize: 16, marginBottom: 6 } },
      'Draggable card (Gesture Handler 3 + Reanimated)'
    ),
    h(
      Pressable,
      {
        onPress: busy,
        accessibilityLabel: 'Block JS 400 ms',
        style: {
          backgroundColor: '#1b5b61',
          borderRadius: 8,
          padding: 8,
          alignSelf: 'flex-start',
          marginBottom: 8,
        },
      },
      h(Text, { style: { color: INK, fontSize: 13 } }, 'Block JS 400 ms')
    ),
    h(
      GestureHandlerRootView,
      { style: { height: 180 } },
      h(
        GestureDetector,
        { gesture: pan },
        h(
          Animated.View,
          {
            style: [
              {
                width: 140,
                height: 96,
                borderRadius: 14,
                backgroundColor: ACCENT,
                margin: 24,
                justifyContent: 'center',
                alignItems: 'center',
              },
              cardStyle,
            ],
          },
          h(
            GestureDetector,
            { gesture: taps_ },
            h(
              View,
              null,
              h(Text, { style: { color: '#06202a', fontSize: 15 } }, 'Drag me, tapped ' + taps)
            )
          )
        )
      )
    ),
    h(Text, { style: { color: MUTED, fontSize: 12 } }, status)
  );
}

module.exports = { SpringCard };
