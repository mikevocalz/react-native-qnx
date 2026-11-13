// Navigation only: an account header and the five destinations. No component
// cards belong in the drawer; those live in the tabs.
import * as React from 'react';
import { Pressable, Text, View } from 'react-native';
import { usePathname, useRouter } from 'expo-router';
import { ACCENT, BACKGROUND, INK, MUTED, SELECTED } from '../theme';

const ITEMS = [
  {
    id: 'home',
    label: 'Home',
    glyph: '\u2302',
    href: '/',
    match: (p) => p === '/' || p === '/index',
  },
  {
    id: 'settings',
    label: 'Settings',
    glyph: '\u2699',
    href: '/settings',
    match: (p) => p === '/settings',
  },
  { id: 'about', label: 'About', glyph: '\u2139', href: '/about', match: (p) => p === '/about' },
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

export default function DrawerContent({ navigation }) {
  const router = useRouter();
  const pathname = usePathname();
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
        <Text style={{ color: INK, fontSize: 18, lineHeight: 24 }}>{__APP_NAME__}</Text>
        <Text style={{ color: MUTED, fontSize: 13, lineHeight: 18 }}>
          Signed in as you@example.com
        </Text>
      </View>
      {ITEMS.map((item) => (
        <DrawerRow
          key={item.id}
          item={item}
          active={item.match(pathname)}
          onPress={() => {
            router.navigate(item.href);
            navigation.closeDrawer?.();
          }}
        />
      ))}
    </View>
  );
}
