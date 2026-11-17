// Real-package fixture for the linked Qt/Hermes host test. Native code drives
// the Qt window and checks rendered pixels; these reports only synchronize it.
const React = require('react');
const { AppRegistry, View, findNodeHandle } = require('react-native');
const Animated = require('react-native-reanimated').default;
const { useSharedValue, useAnimatedStyle, withTiming } = require('react-native-reanimated');
const { scheduleOnUI, scheduleOnRN } = require('react-native-worklets');
const {
  GestureHandlerRootView,
  GestureDetector,
  usePanGesture,
} = require('react-native-gesture-handler');

const h = React.createElement;
globalThis.__rnqnxIntegration = {};

function record(name, value) {
  globalThis.__rnqnxIntegration[name] = value;
}

function probeFrames(answer) {
  'worklet';
  const role = globalThis.__rnqnxThreadRole();
  let frames = 0;
  let previous = -1;
  let monotonic = true;
  // This closure is created on the UI runtime. Serializing it again would
  // break its recursive reference and its per-probe counter.
  function tick(timestamp) {
    monotonic = monotonic && timestamp >= previous;
    previous = timestamp;
    frames += 1;
    if (frames < 3) requestAnimationFrame(tick);
    else scheduleOnRN(record, 'frames', { answer, role, count: frames, monotonic });
  }
  requestAnimationFrame(tick);
}

function queuePendingFrame(admission) {
  'worklet';
  requestAnimationFrame(() => {
    admission.value = 100;
  });
  admission.value = 99;
}

function WorkletsIntegration() {
  const timing = useSharedValue(0);
  const translation = useSharedValue(0);
  const admission = useSharedValue(0);
  const timingRef = React.useRef(null);
  const gestureRef = React.useRef(null);
  const timingStyle = useAnimatedStyle(() => ({ transform: [{ translateX: timing.value }] }));
  const gestureStyle = useAnimatedStyle(() => ({ transform: [{ translateX: translation.value }] }));
  const pan = usePanGesture({
    minDistance: 5,
    onUpdate: (event) => {
      'worklet';
      translation.value = event.translationX;
      scheduleOnRN(record, 'panUpdate', event.translationX);
    },
    onDeactivate: (event) => {
      'worklet';
      scheduleOnRN(record, 'panEnd', event.translationX);
    },
  });

  React.useEffect(() => {
    globalThis.__rnqnxStartTiming = () => {
      timing.value = withTiming(60, { duration: 80 }, (finished) => {
        'worklet';
        scheduleOnRN(record, 'timingDone', finished === true);
      });
    };
    globalThis.__rnqnxQueueBeforeTeardown = () => scheduleOnUI(queuePendingFrame, admission);
    globalThis.__rnqnxTeardownAdmission = () => admission.value;
    record('timingTag', findNodeHandle(timingRef.current));
    record('gestureTag', findNodeHandle(gestureRef.current));
    record('ready', true);
    scheduleOnUI(probeFrames, 42);
    return () => {
      record('unmounted', true);
      delete globalThis.__rnqnxStartTiming;
      delete globalThis.__rnqnxQueueBeforeTeardown;
      delete globalThis.__rnqnxTeardownAdmission;
    };
  }, [timing, admission]);

  return h(
    GestureHandlerRootView,
    { style: { flex: 1 } },
    h(
      View,
      { style: { flex: 1, backgroundColor: '#ffffff' } },
      h(Animated.View, {
        ref: timingRef,
        collapsable: false,
        style: [
          {
            position: 'absolute',
            left: 20,
            top: 20,
            width: 40,
            height: 40,
            backgroundColor: '#ff0000',
          },
          timingStyle,
        ],
      }),
      h(
        GestureDetector,
        { gesture: pan },
        h(Animated.View, {
          ref: gestureRef,
          collapsable: false,
          style: [
            {
              position: 'absolute',
              left: 20,
              top: 100,
              width: 40,
              height: 40,
              backgroundColor: '#0000ff',
            },
            gestureStyle,
          ],
        })
      )
    )
  );
}

AppRegistry.registerComponent('RnqnxWorkletsIntegration', () => WorkletsIntegration);
