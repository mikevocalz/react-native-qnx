// Drawer destination: where each part of the gallery lives. Navigation only;
// the cards themselves are in the tabs.
import * as React from 'react';
import { ScrollView, Text, View } from 'react-native';
import { Link } from 'expo-router';
import { Screen, Heading, INK, MUTED, PANEL } from '../ui';

const DESTINATIONS = [
  [
    '/',
    'Components',
    'View, Text, Image, Pressable, TextInput, Switch, Modal, expo-image, Expo modules',
  ],
  ['/advanced', 'Advanced', 'One card per file in components/advanced: SVG, long list and more'],
  ['/documents', 'Documents', '1,000 synthetic rows in a virtualized FlatList'],
  ['/controls', 'Controls', 'Controlled TextInput, Switch and Pressable state'],
  ['/more', 'More', 'SectionList, adaptive layout and the list of known limits'],
];

export default function ComponentsRoute() {
  return (
    <Screen>
      <Heading title="Components" detail="The tabs hold the cards. Pick one to open it." />
      <ScrollView style={{ flex: 1 }}>
        {DESTINATIONS.map(([href, title, detail]) => (
          <Link key={href} href={href} asChild>
            <View
              accessibilityRole="link"
              accessible
              style={{ backgroundColor: PANEL, borderRadius: 12, padding: 18, marginBottom: 10 }}
            >
              <Text style={{ color: INK, fontSize: 17, lineHeight: 24 }}>{title}</Text>
              <Text style={{ color: MUTED, fontSize: 14, lineHeight: 20 }}>{detail}</Text>
            </View>
          </Link>
        ))}
      </ScrollView>
    </Screen>
  );
}
