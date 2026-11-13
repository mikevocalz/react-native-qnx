import * as React from 'react';
import { ScrollView, View } from 'react-native';
import { Screen, Heading, PANEL } from '../ui';
import ExpoImageCard from '../ExpoImageCard';
import { MediaPage } from '../media/MediaPage';

export default function MediaRoute() {
  return (
    <Screen>
      <Heading title="Media" detail="Images from bundled files and URLs through expo-image." />
      <ScrollView style={{ flex: 1 }}>
        <View style={{ backgroundColor: PANEL, borderRadius: 14, padding: 22, maxWidth: 640 }}>
          <ExpoImageCard />
        </View>
        <View style={{ marginTop: 20 }}>
          <MediaPage width={720} />
        </View>
      </ScrollView>
    </Screen>
  );
}
