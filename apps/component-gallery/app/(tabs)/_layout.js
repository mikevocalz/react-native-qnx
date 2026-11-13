// The five tabs. A bottom bar on narrow windows and a left rail from 700 px
// up, switched live from the measured width. The bar itself is the native Qt
// tab bar when the host has one (see @rn-qnx/expo-router-qnx).
import * as React from 'react';
import { View } from 'react-native';
import { Tabs } from 'expo-router';
import { qnxTabsScreenOptions } from '@rn-qnx/expo-router-qnx';
import { ACCENT, BACKGROUND, MUTED } from '../../ui';

const TABS = [
  { name: 'index', title: 'Components', icon: '▦' },
  { name: 'advanced', title: 'Advanced', icon: '✦' },
  { name: 'documents', title: 'Documents', icon: '☰' },
  { name: 'controls', title: 'Controls', icon: '⚙' },
  { name: 'more', title: 'More', icon: '⋯' },
];

export default function TabsLayout() {
  const [width, setWidth] = React.useState(1280);
  return (
    <View
      style={{ flex: 1 }}
      onLayout={(event) => {
        const next = Math.round(event.nativeEvent.layout.width);
        if (next > 0) setWidth(next);
      }}
    >
      <Tabs
        screenOptions={qnxTabsScreenOptions({
          width,
          headerShown: false,
          tabBarActiveTintColor: ACCENT,
          tabBarInactiveTintColor: MUTED,
          tabBarStyle: { backgroundColor: '#102338' },
          sceneStyle: { backgroundColor: BACKGROUND },
        })}
      >
        {TABS.map((tab) => (
          <Tabs.Screen
            key={tab.name}
            name={tab.name}
            options={{ title: tab.title, qnxIcon: tab.icon }}
          />
        ))}
      </Tabs>
    </View>
  );
}
