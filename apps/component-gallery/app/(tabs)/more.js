// The fifth tab. Sections, Adaptive and Limits, which were tabs of their own
// before the gallery moved to expo-router, are pages of this one.
import * as React from 'react';
import { Pressable, Text, View } from 'react-native';
import { Screen, MUTED, SELECTED } from '../../ui';
import { SectionsScreen } from '../../screens/SectionsScreen';
import { AdaptiveScreen } from '../../screens/AdaptiveScreen';
import { LimitsScreen } from '../../screens/LimitsScreen';

const PAGES = [
  { id: 'sections', title: 'Sections', Page: SectionsScreen },
  { id: 'adaptive', title: 'Adaptive', Page: AdaptiveScreen },
  { id: 'limits', title: 'Limits', Page: LimitsScreen },
];

export default function MoreRoute() {
  const [page, setPage] = React.useState('sections');
  const current = PAGES.find((candidate) => candidate.id === page) ?? PAGES[0];
  return (
    <Screen>
      <View style={{ flexDirection: 'row', marginBottom: 14 }}>
        {PAGES.map((candidate) => (
          <Pressable
            key={candidate.id}
            accessibilityRole="button"
            accessibilityLabel={candidate.title}
            onPress={() => setPage(candidate.id)}
            style={{
              minHeight: 34,
              paddingHorizontal: 14,
              justifyContent: 'center',
              borderRadius: 9,
              marginRight: 6,
              backgroundColor: candidate.id === page ? SELECTED : 'transparent',
            }}
          >
            <Text style={{ color: candidate.id === page ? '#f2ffff' : MUTED, fontSize: 15 }}>
              {candidate.title}
            </Text>
          </Pressable>
        ))}
      </View>
      <View style={{ flex: 1, minHeight: 0 }}>
        <current.Page />
      </View>
    </Screen>
  );
}
