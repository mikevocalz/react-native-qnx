import * as React from 'react';
import { ScrollView, View } from 'react-native';
import { Screen, Heading, PANEL, useViewport } from '../ui';
import ExpoImageCard from '../ExpoImageCard';
import { MediaPage } from '../media/MediaPage';

// Inside the Screen, so useViewport reads this page's own width: the page
// width minus its 28 px padding on each side, not a fixed 720.
function MediaBody() {
  const { width } = useViewport();
  const pageWidth = Math.max(240, width - 56);
  return (
    <ScrollView style={{ flex: 1 }}>
      <View style={{ backgroundColor: PANEL, borderRadius: 14, padding: 22, maxWidth: 640 }}>
        <ExpoImageCard />
      </View>
      <View style={{ marginTop: 20 }}>
        <MediaPage width={pageWidth} />
      </View>
    </ScrollView>
  );
}

export default function MediaRoute() {
  return (
    <Screen>
      <Heading title="Media" detail="Images from bundled files and URLs through expo-image." />
      <MediaBody />
    </Screen>
  );
}
