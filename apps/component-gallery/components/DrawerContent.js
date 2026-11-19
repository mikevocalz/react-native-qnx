// Navigation only: an account header and the five destinations. No component
// cards belong in the drawer; those live in the tabs.
import * as React from 'react';
import { Pressable, Text, View } from 'react-native';
import { useRouter } from 'expo-router';
import Constants from 'expo-constants';
import { ACCENT, BACKGROUND, INK, MUTED, SELECTED } from '../ui';

const ITEMS = [
  { id: 'home', label: 'Home', glyph: '⌂', href: '/', route: '(tabs)' },
  {
    id: 'components',
    label: 'Components',
    glyph: '▦',
    href: '/components',
    route: 'components',
  },
  { id: 'media', label: 'Media', glyph: '▶', href: '/media', route: 'media' },
  {
    id: 'settings',
    label: 'Settings',
    glyph: '⚙',
    href: '/settings',
    route: 'settings',
  },
  { id: 'about', label: 'About', glyph: '\u2139', href: '/about', route: 'about' },
];

function DrawerRow({ item, active, onPress }) {
  return (
    <Pressable
      accessibilityRole="button"
      accessibilityLabel={item.label}
      accessibilityState={{ selected: active }}
      onPress={onPress}
      style={({ pressed, hovered }) => ({
        flexDirection: 'row',
        alignItems: 'center',
        height: 46,
        marginHorizontal: 10,
        paddingHorizontal: 14,
        borderRadius: 10,
        backgroundColor: active
          ? SELECTED
          : pressed
            ? '#1b3550'
            : hovered
              ? '#162b42'
              : 'transparent',
      })}
    >
      <Text style={{ color: active ? '#f2ffff' : ACCENT, fontSize: 18, width: 30 }}>
        {item.glyph}
      </Text>
      <Text style={{ color: active ? '#f2ffff' : INK, fontSize: 16 }}>{item.label}</Text>
    </Pressable>
  );
}

export default function DrawerContent({ navigation, state }) {
  const router = useRouter();
  // Child tabs change the URL while the active drawer destination stays Home.
  const activeRoute = state.routes[state.index].name;
  const name = Constants.expoConfig?.name ?? 'QNX Components';
  return (
    <View style={{ flex: 1, backgroundColor: '#0f2236' }}>
      <View style={{ padding: 20, paddingTop: 28, backgroundColor: BACKGROUND, marginBottom: 12 }}>
        <View
          style={{
            width: 52,
            height: 52,
            borderRadius: 26,
            backgroundColor: SELECTED,
            alignItems: 'center',
            justifyContent: 'center',
            marginBottom: 12,
          }}
        >
          <Text style={{ color: '#f2ffff', fontSize: 22 }}>Q</Text>
        </View>
        <Text style={{ color: INK, fontSize: 18, lineHeight: 24 }}>{name}</Text>
        <Text style={{ color: MUTED, fontSize: 13, lineHeight: 18 }}>
          signed in as developer@rnqnx.local
        </Text>
      </View>
      {ITEMS.map((item) => (
        <DrawerRow
          key={item.id}
          item={item}
          active={item.route === activeRoute}
          onPress={() => {
            router.navigate(item.href);
            navigation.closeDrawer?.();
          }}
        />
      ))}
    </View>
  );
}
