// Tabs: a bottom bar on narrow windows and a left rail from 700 px up,
// switched live from the measured width. At most five tabs; put the rest
// under a More tab.
import * as React from 'react';
import { View } from 'react-native';
import { Tabs } from 'expo-router';
import { qnxTabsScreenOptions } from '@rn-qnx/expo-router-qnx';
import { ACCENT, BACKGROUND, MUTED } from '../../theme';

const TABS = [
  { name: 'index', title: 'Home', icon: '⌂' },
  { name: 'explore', title: 'Explore', icon: '✦' },
  { name: 'controls', title: 'Controls', icon: '⚙' },
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
