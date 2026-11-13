// Root layout: a left drawer (navigation only) around the tabs group.
import * as React from 'react';
import { DarkTheme, ThemeProvider } from 'expo-router';
import { Drawer } from 'expo-router/drawer';
import AppHeader from '../components/AppHeader';
import DrawerContent from '../components/DrawerContent';
import { BACKGROUND } from '../theme';

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
        <Drawer.Screen name="(tabs)" options={{ title: 'Home', header: header(__APP_NAME__) }} />
        <Drawer.Screen
          name="settings"
          options={{ title: 'Settings', header: header('Settings') }}
        />
        <Drawer.Screen name="about" options={{ title: 'About', header: header('About') }} />
      </Drawer>
    </ThemeProvider>
  );
}
