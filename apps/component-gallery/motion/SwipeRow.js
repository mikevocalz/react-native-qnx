// A swipeable list row: drag left to reveal an action, release past the
// threshold to delete, otherwise it springs back. Pan activates after 12 px
// horizontally and fails if the pointer moves 12 px vertically first, so a
// vertical scroll over the row is not taken.
const React = require('react');
const { View, Text } = require('react-native');
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
} = require('react-native-gesture-handler');
const { scheduleOnRN } = require('react-native-worklets');

const h = React.createElement;
const INK = '#e9f0f7';
const MUTED = '#9dafc2';
const PANEL = '#142538';
const THRESHOLD = -120;

function Row({ label, onDelete }) {
  const x = useSharedValue(0);
  const start = useSharedValue(0);
  const height = useSharedValue(64);
  const pan = usePanGesture({
    activeOffsetX: [-12, 12],
    failOffsetY: [-12, 12],
    onBegin: () => {
      'worklet';
      start.value = x.value;
    },
    onUpdate: (event) => {
      'worklet';
      x.value = Math.min(0, start.value + event.translationX);
    },
    onDeactivate: () => {
      'worklet';
      if (x.value < THRESHOLD) {
        x.value = withTiming(-420, { duration: 160 });
        height.value = withTiming(0, { duration: 200 }, (finished) => {
          'worklet';
          if (finished) scheduleOnRN(onDelete, label);
        });
      } else {
        x.value = withSpring(0, { damping: 16, stiffness: 220 });
      }
    },
  });
  const rowStyle = useAnimatedStyle(() => ({ transform: [{ translateX: x.value }] }));
  const wrapStyle = useAnimatedStyle(() => ({ height: height.value, overflow: 'hidden' }));
  const actionStyle = useAnimatedStyle(() => ({ opacity: Math.min(1, -x.value / 80) }));
  return h(
    Animated.View,
    { style: [{ marginBottom: 6 }, wrapStyle] },
    h(
      Animated.View,
      {
        style: [
          {
            position: 'absolute',
            right: 0,
            top: 0,
            bottom: 0,
            width: 140,
            backgroundColor: '#8f2d3a',
            borderRadius: 8,
            justifyContent: 'center',
            alignItems: 'flex-end',
            paddingRight: 18,
          },
          actionStyle,
        ],
      },
      h(Text, { style: { color: INK } }, 'Delete')
    ),
    h(
      GestureDetector,
      { gesture: pan },
      h(
        Animated.View,
        {
          style: [
            {
              height: 58,
              backgroundColor: '#1d3a52',
              borderRadius: 8,
              justifyContent: 'center',
              paddingHorizontal: 14,
            },
            rowStyle,
          ],
        },
        h(Text, { style: { color: INK, fontSize: 15 } }, label),
        h(Text, { style: { color: MUTED, fontSize: 12 } }, 'drag left to delete')
      )
    )
  );
}

function SwipeRow() {
  const [rows, setRows] = React.useState(['Inbox', 'Drafts', 'Sent', 'Archive']);
  const [deleted, setDeleted] = React.useState('none yet');
  const remove = React.useCallback((label) => {
    setRows((previous) => previous.filter((row) => row !== label));
    setDeleted(label);
  }, []);
  return h(
    View,
    { style: { backgroundColor: PANEL, borderRadius: 12, padding: 16, marginTop: 16 } },
    h(Text, { style: { color: INK, fontSize: 16, marginBottom: 8 } }, 'Swipeable list items'),
    h(
      GestureHandlerRootView,
      null,
      ...rows.map((label) => h(Row, { key: label, label, onDelete: remove }))
    ),
    h(Text, { style: { color: MUTED, fontSize: 12 } }, 'last deleted: ' + deleted)
  );
}

module.exports = { SwipeRow };
