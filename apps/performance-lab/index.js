// Fixed workload, not a benchmark result. No network, premium modules or JS FPS claims.
const React = require('react');
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const View = require('react-native/Libraries/Components/View/View').default;
const Text = require('react-native/Libraries/Text/Text').default;
const FlatList = require('react-native/Libraries/Lists/FlatList').default;
const { createRows } = require('./fixtures.js');

const rows = createRows();
const h = React.createElement;
const ROW_HEIGHT = 48;

function PerformanceList() {
  return h(
    View,
    {
      collapsable: false,
      style: { flex: 1, width: '100%', height: '100%', backgroundColor: '#102033' },
    },
    h(
      Text,
      { style: { color: '#ffffff', fontSize: 18, padding: 12 } },
      'RNQNX performance workload / list-1000'
    ),
    h(FlatList, {
      style: { flex: 1 },
      data: rows,
      initialNumToRender: 12,
      maxToRenderPerBatch: 12,
      windowSize: 5,
      keyExtractor: (item) => item.id,
      getItemLayout: (_data, index) => ({
        length: ROW_HEIGHT,
        offset: ROW_HEIGHT * index,
        index,
      }),
      renderItem: ({ item }) =>
        h(
          View,
          {
            collapsable: false,
            style: { height: ROW_HEIGHT, paddingHorizontal: 12, justifyContent: 'center' },
          },
          h(Text, { style: { color: '#e3edf7', fontSize: 16 } }, item.label)
        ),
    })
  );
}

AppRegistry.registerComponent('RNQnxPlayground', () => PerformanceList);
