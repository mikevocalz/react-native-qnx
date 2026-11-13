// A long virtualized FlatList: 5,000 rows, of which only the visible window
// is mounted as Qt Quick items.
import * as React from 'react';
import { FlatList, Text, View } from 'react-native';

const ROWS = Array.from({ length: 5000 }, (_, index) => ({
  key: String(index),
  title: `Row ${index + 1}`,
  detail: index % 3 === 0 ? 'Mounted on demand by VirtualizedList' : 'Recycled window',
}));

function LongList() {
  // The fixed-height, clipped frame keeps the list's 220 px viewport inside
  // the card when the Advanced page itself is a list.
  return (
    <View style={{ height: 220, overflow: 'hidden', borderRadius: 10 }}>
      <FlatList
        data={ROWS}
        style={{ flex: 1, backgroundColor: '#0c1929' }}
        initialNumToRender={12}
        windowSize={5}
        getItemLayout={(_, index) => ({ length: 44, offset: 44 * index, index })}
        renderItem={({ item }) => (
          <View
            style={{
              height: 44,
              paddingHorizontal: 14,
              flexDirection: 'row',
              alignItems: 'center',
              borderBottomWidth: 1,
              borderBottomColor: '#142538',
            }}
          >
            <Text style={{ color: '#e9f0f7', fontSize: 15, width: 110 }}>{item.title}</Text>
            <Text style={{ color: '#9dafc2', fontSize: 13 }}>{item.detail}</Text>
          </View>
        )}
      />
    </View>
  );
}

export default {
  id: 'long-list',
  title: 'Long FlatList',
  detail: '5,000 rows, virtualized',
  order: 50,
  Component: LongList,
};
