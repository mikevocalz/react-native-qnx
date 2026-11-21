// VirtualizedSectionList on its own, imported from the public entry (issue #178).
import * as React from 'react';
import { Text, View } from 'react-native';
import { INK, ACCENT } from '../../ui';
import { ProofShell, resolveComponent } from './proofCard';

const SECTIONS = [
  { title: 'Alpha', data: ['a1', 'a2', 'a3'] },
  { title: 'Beta', data: ['b1', 'b2'] },
];

function VirtualizedSectionListProof() {
  const resolved = React.useMemo(() => resolveComponent('VirtualizedSectionList', []), []);
  return (
    <ProofShell name="VirtualizedSectionList" resolved={resolved}>
      {(VirtualizedSectionList) => (
        <View
          style={{ height: 170, borderRadius: 10, overflow: 'hidden', backgroundColor: '#0c1929' }}
        >
          <VirtualizedSectionList
            sections={SECTIONS}
            getItemCount={(items) => items.length}
            getItem={(items, index) => items[index]}
            keyExtractor={(item) => item}
            renderSectionHeader={({ section }) => (
              <Text style={{ color: ACCENT, fontSize: 13, padding: 8, backgroundColor: '#142538' }}>
                {section.title}
              </Text>
            )}
            renderItem={({ item }) => (
              <Text style={{ color: INK, fontSize: 14, padding: 8 }}>{item}</Text>
            )}
          />
        </View>
      )}
    </ProofShell>
  );
}

export default {
  id: 'virtualized-section-list',
  title: 'VirtualizedSectionList',
  detail: 'Base list under SectionList',
  order: 72,
  component: 'VirtualizedSectionList',
  Component: VirtualizedSectionListProof,
};
