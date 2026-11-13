// The header the drawer screens use: a menu button that toggles the drawer,
// and the screen title. Drawn here rather than with react-navigation's Header
// so it matches the gallery palette and needs no image assets.
import * as React from 'react';
import { Pressable, Text, View } from 'react-native';
import { useNavigation, useRouter } from 'expo-router';
import { DrawerActions } from 'expo-router/react-navigation';
import { ACCENT, BACKGROUND, INK, MUTED } from '../ui';
import { registerDevMenuActions } from '../devMenu';

export default function AppHeader({ title, subtitle }) {
  const navigation = useNavigation();
  const router = useRouter();
  React.useEffect(() => {
    registerDevMenuActions({
      toggleDrawer: () => navigation.dispatch(DrawerActions.toggleDrawer()),
      navigate: (href) => router.navigate(href),
    });
  }, [navigation, router]);
  return (
    <View
      style={{
        flexDirection: 'row',
        alignItems: 'center',
        height: 56,
        paddingHorizontal: 12,
        backgroundColor: BACKGROUND,
        borderBottomWidth: 1,
        borderBottomColor: 'rgba(255,255,255,0.08)',
      }}
    >
      <Pressable
        accessibilityRole="button"
        accessibilityLabel="Open navigation drawer"
        onPress={() => navigation.dispatch(DrawerActions.toggleDrawer())}
        style={({ pressed, hovered }) => ({
          width: 40,
          height: 40,
          borderRadius: 10,
          alignItems: 'center',
          justifyContent: 'center',
          backgroundColor: pressed ? '#237a80' : hovered ? 'rgba(255,255,255,0.08)' : 'transparent',
        })}
      >
        <Text style={{ color: INK, fontSize: 22, lineHeight: 26 }}>{'☰'}</Text>
      </Pressable>
      <View style={{ marginLeft: 12 }}>
        {subtitle ? (
          <Text style={{ color: ACCENT, fontSize: 11, lineHeight: 14 }}>{subtitle}</Text>
        ) : null}
        <Text style={{ color: INK, fontSize: 20, lineHeight: 26 }}>{title}</Text>
      </View>
      <View style={{ flex: 1 }} />
      {__DEV__ ? <Text style={{ color: MUTED, fontSize: 12 }}>Fast Refresh on</Text> : null}
    </View>
  );
}
