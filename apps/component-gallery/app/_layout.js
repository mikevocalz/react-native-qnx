// Root layout: a left drawer around the tabs group, as in the expo-router tabs
// template with a drawer added. The drawer is navigation only.
import * as React from 'react';
import { DarkTheme, ThemeProvider } from 'expo-router';
import { Drawer } from 'expo-router/drawer';
import AppHeader from '../components/AppHeader';
import DrawerContent from '../components/DrawerContent';
import { BACKGROUND } from '../ui';

const theme = {
  ...DarkTheme,
  colors: { ...DarkTheme.colors, background: BACKGROUND, card: '#0f2236', border: '#1d3248' },
};

const header = (title) => () => <AppHeader title={title} />;

export default function RootLayout() {
  return (
    <ThemeProvider value={theme}>
      <Drawer
        drawerContent={(props) => <DrawerContent {...props} />}
        screenOptions={{ drawerType: 'front', sceneStyle: { backgroundColor: BACKGROUND } }}
      >
        <Drawer.Screen
          name="(tabs)"
          options={{ title: 'Home', header: header('Component gallery') }}
        />
        <Drawer.Screen
          name="components"
          options={{ title: 'Components', header: header('Components') }}
        />
        <Drawer.Screen name="media" options={{ title: 'Media', header: header('Media') }} />
        <Drawer.Screen
          name="settings"
          options={{ title: 'Settings', header: header('Settings') }}
        />
        <Drawer.Screen name="about" options={{ title: 'About', header: header('About') }} />
      </Drawer>
    </ThemeProvider>
  );
}
