import * as React from 'react';
import { useWindowDimensions } from 'react-native';
import { Tabs } from 'expo-router';
import { qnxTabsScreenOptions } from '@rn-qnx/expo-router-qnx';

export default function Layout() {
  const { width } = useWindowDimensions();
  return (
    <Tabs screenOptions={qnxTabsScreenOptions({ width, headerShown: false })}>
      <Tabs.Screen
        name="index"
        options={{ title: 'Draft', qnxIcon: 'D', tabBarAccessibilityLabel: 'Draft tab' }}
      />
      <Tabs.Screen
        name="review"
        options={{ title: 'Review', qnxIcon: 'R', tabBarAccessibilityLabel: 'Review tab' }}
      />
    </Tabs>
  );
}
