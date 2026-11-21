// VirtualizedList on its own, imported from the public entry (issue #178).
import * as React from 'react';
import { Text, View } from 'react-native';
import { INK, MUTED } from '../../ui';
import { ProofShell, resolveComponent } from './proofCard';

const DATA = Array.from({ length: 200 }, (_, index) => ({
  key: String(index),
  label: `Item ${index + 1}`,
}));

function VirtualizedListProof() {
  const resolved = React.useMemo(() => resolveComponent('VirtualizedList', []), []);
  return (
    <ProofShell name="VirtualizedList" resolved={resolved}>
      {(VirtualizedList) => (
        <View
          style={{ height: 170, borderRadius: 10, overflow: 'hidden', backgroundColor: '#0c1929' }}
        >
          <VirtualizedList
            data={DATA}
            initialNumToRender={8}
            getItemCount={(data) => data.length}
            getItem={(data, index) => data[index]}
            keyExtractor={(item) => item.key}
            renderItem={({ item }) => (
              <View style={{ height: 34, justifyContent: 'center', paddingHorizontal: 12 }}>
                <Text style={{ color: INK, fontSize: 14 }}>{item.label}</Text>
                <Text style={{ color: MUTED, fontSize: 10 }}>row from {DATA.length}</Text>
              </View>
            )}
          />
        </View>
      )}
    </ProofShell>
  );
}

export default {
  id: 'virtualized-list',
  title: 'VirtualizedList',
  detail: 'Base list under FlatList, 200 rows',
  order: 70,
  component: 'VirtualizedList',
  Component: VirtualizedListProof,
};
