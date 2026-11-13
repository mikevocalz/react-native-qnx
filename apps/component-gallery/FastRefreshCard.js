// Fast Refresh demo. This file exports only a component, so React Refresh can
// swap it in place: edit HEADLINE or ACCENT, save, and the card on the device
// changes while the counter below keeps its value. Editing index.js instead
// reloads the whole app, because that module registers the root component.
import * as React from 'react';
import { DevSettings, Pressable, Text, View } from 'react-native';

const HEADLINE = 'Edit FastRefreshCard.js and save';
const ACCENT = '#79e8dc';

export default function FastRefreshCard() {
  const [count, setCount] = React.useState(0);
  // Developer menu entry (Ctrl+D or `d` in Metro): the counter can be bumped
  // from the keyboard too. DevSettings is a no-op in release builds.
  React.useEffect(() => {
    DevSettings.addMenuItem('Fast Refresh card: add 1', () => setCount((value) => value + 1));
  }, []);
  const dev = typeof __DEV__ !== 'undefined' && __DEV__;
  return (
    <View>
      <Text style={{ color: ACCENT, fontSize: 20, lineHeight: 26, marginBottom: 6 }}>
        {HEADLINE}
      </Text>
      <Text style={{ color: '#9dafc2', fontSize: 14, lineHeight: 20, marginBottom: 14 }}>
        {dev
          ? 'Development client: Metro sends each save over /hot. Ctrl+R reloads, Ctrl+D opens the developer menu.'
          : 'Release build: bundled bytecode, no Metro connection. Run rnqnx project dev for Fast Refresh.'}
      </Text>
      <Pressable
        onPress={() => setCount((value) => value + 1)}
        accessibilityRole="button"
        accessibilityLabel="Fast Refresh counter"
        style={({ pressed }) => ({
          alignSelf: 'flex-start',
          minHeight: 44,
          paddingHorizontal: 20,
          justifyContent: 'center',
          borderRadius: 10,
          backgroundColor: pressed ? '#237a80' : '#1b5b61',
        })}
      >
        <Text style={{ color: '#f2ffff', fontSize: 15 }}>
          {'State kept across edits: ' + count}
        </Text>
      </Pressable>
    </View>
  );
}
