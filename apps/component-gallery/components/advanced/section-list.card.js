import * as React from 'react';
import { Text, View } from 'react-native';
import { INK, MUTED, ACCENT } from '../../ui';
import { ProofShell, resolveComponent } from './proofCard';

const SECTIONS = [
  { title: 'Fruit', data: ['Apple', 'Banana', 'Cherry'] },
  { title: 'Vegetables', data: ['Carrot', 'Leek'] },
  { title: 'Grains', data: ['Oat', 'Rye', 'Barley'] },
];

function SectionListProof() {
  const resolved = React.useMemo(() => resolveComponent('SectionList', []), []);
  return (
    <ProofShell name="SectionList" resolved={resolved}>
      {(SectionList) => (
        <View
          style={{ height: 190, borderRadius: 10, overflow: 'hidden', backgroundColor: '#0c1929' }}
        >
          <SectionList
            sections={SECTIONS}
            keyExtractor={(item) => item}
            renderSectionHeader={({ section }) => (
              <Text style={{ color: ACCENT, fontSize: 13, padding: 8, backgroundColor: '#142538' }}>
                {section.title}
              </Text>
            )}
            renderItem={({ item }) => (
              <Text style={{ color: INK, fontSize: 14, padding: 8 }}>{item}</Text>
            )}
            ItemSeparatorComponent={() => (
              <View style={{ height: 1, backgroundColor: '#142538' }} />
            )}
            ListFooterComponent={
              <Text style={{ color: MUTED, fontSize: 11, padding: 8 }}>end of list</Text>
            }
          />
        </View>
      )}
    </ProofShell>
  );
}

export default {
  id: 'section-list',
  title: 'SectionList',
  detail: 'Three sections with headers',
  order: 71,
  component: 'SectionList',
  Component: SectionListProof,
};
