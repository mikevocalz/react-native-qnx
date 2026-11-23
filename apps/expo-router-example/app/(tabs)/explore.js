import * as React from 'react';
import { ScrollView, Text, View } from 'react-native';
import { BACKGROUND, INK, MUTED, PANEL } from '../../theme';

export default function ExploreRoute() {
  return (
    <View style={{ flex: 1, backgroundColor: BACKGROUND, padding: 28 }}>
      <Text style={{ color: INK, fontSize: 24, lineHeight: 32 }}>Explore</Text>
      <Text style={{ color: MUTED, fontSize: 14, lineHeight: 20, marginBottom: 16 }}>
        The second tab.
      </Text>
      <ScrollView style={{ flex: 1 }}>
        <View style={{ backgroundColor: PANEL, borderRadius: 14, padding: 22 }}>
          <Text style={{ color: INK, fontSize: 16, lineHeight: 24 }}>
            Edit app/(tabs)/explore.js and save: Fast Refresh swaps this screen in place.
          </Text>
        </View>
      </ScrollView>
    </View>
  );
}
