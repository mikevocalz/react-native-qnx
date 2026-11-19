// @legendapp/list on the QNX host: 5000 variable-height rows over ScrollView.
const React = require('react');
const { View, Text, Pressable } = require('react-native');
const { LegendList } = require('@legendapp/list/react-native');

const h = React.createElement;
const INK = '#e9f0f7';
const MUTED = '#9dafc2';
const ACCENT = '#79e8dc';
const PANEL = '#142538';
const COUNT = 5000;

const DATA = Array.from({ length: COUNT }, (_, index) => ({
  id: 'row-' + index,
  title: 'Row ' + index,
  // 1 to 4 lines so row heights vary with content.
  body: 'Variable height content. '.repeat(1 + ((index * 7) % 4)),
  header: index % 250 === 0,
}));

function Chip({ label, onPress }) {
  return h(
    Pressable,
    {
      onPress,
      accessibilityLabel: label,
      style: {
        backgroundColor: '#1b5b61',
        borderRadius: 8,
        padding: 8,
        marginRight: 8,
        marginBottom: 6,
      },
    },
    h(Text, { style: { color: INK, fontSize: 13 } }, label)
  );
}

function LegendListCard() {
  const listRef = React.useRef(null);
  const [recycle, setRecycle] = React.useState(true);
  const [stats, setStats] = React.useState({ offset: 0, layouts: 0, viewable: '-' });

  const renderItem = React.useCallback(
    ({ item, index }) =>
      h(
        View,
        {
          style: {
            padding: 12,
            borderBottomWidth: 1,
            borderBottomColor: '#1d3a52',
            backgroundColor: item.header ? '#1b3a4b' : PANEL,
          },
        },
        h(
          Text,
          { style: { color: item.header ? ACCENT : INK, fontSize: 15 } },
          item.title + (item.header ? '  (sticky header)' : '')
        ),
        h(Text, { style: { color: MUTED, fontSize: 12, marginTop: 4 } }, item.body)
      ),
    []
  );

  return h(
    View,
    { style: { backgroundColor: PANEL, borderRadius: 12, padding: 16, marginTop: 16 } },
    h(
      Text,
      { style: { color: INK, fontSize: 16, marginBottom: 6 } },
      '@legendapp/list 3.7.2, ' + COUNT + ' rows'
    ),
    h(
      View,
      { style: { flexDirection: 'row', flexWrap: 'wrap' } },
      h(Chip, {
        label: 'scrollToIndex 2500',
        onPress: () =>
          listRef.current && listRef.current.scrollToIndex({ index: 2500, animated: false }),
      }),
      h(Chip, {
        label: 'scrollToEnd',
        onPress: () => listRef.current && listRef.current.scrollToEnd({ animated: false }),
      }),
      h(Chip, {
        label: 'scrollToOffset 0',
        onPress: () =>
          listRef.current && listRef.current.scrollToOffset({ offset: 0, animated: false }),
      }),
      h(Chip, {
        label: recycle ? 'recycleItems: on' : 'recycleItems: off',
        onPress: () => setRecycle((value) => !value),
      })
    ),
    h(
      Text,
      { style: { color: ACCENT, fontSize: 12, marginBottom: 6 } },
      'offset ' +
        Math.round(stats.offset) +
        ', onLayout calls ' +
        stats.layouts +
        ', viewable ' +
        stats.viewable
    ),
    h(
      View,
      { style: { height: 360, borderRadius: 8, overflow: 'hidden' } },
      h(LegendList, {
        ref: listRef,
        data: DATA,
        renderItem,
        keyExtractor: (item) => item.id,
        estimatedItemSize: 64,
        recycleItems: recycle,
        maintainVisibleContentPosition: true,
        stickyHeaderIndices: DATA.map((item, index) => (item.header ? index : -1)).filter(
          (index) => index >= 0
        ),
        onScroll: (event) => {
          const offset = event.nativeEvent.contentOffset.y;
          setStats((previous) =>
            Math.abs(previous.offset - offset) < 8 ? previous : { ...previous, offset }
          );
        },
        scrollEventThrottle: 16,
        onLayout: () => setStats((previous) => ({ ...previous, layouts: previous.layouts + 1 })),
        onViewableItemsChanged: ({ viewableItems }) => {
          if (viewableItems.length) {
            setStats((previous) => ({
              ...previous,
              viewable:
                viewableItems[0].index + '..' + viewableItems[viewableItems.length - 1].index,
            }));
          }
        },
        style: { flex: 1 },
      })
    )
  );
}

module.exports = { LegendListCard };
