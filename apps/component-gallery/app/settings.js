import * as React from 'react';
import { Switch, Text, View } from 'react-native';
import { Screen, Heading, INK, MUTED, PANEL, useConformance } from '../ui';

function Row({ label, detail, value, onValueChange }) {
  return (
    <View
      style={{
        flexDirection: 'row',
        alignItems: 'center',
        backgroundColor: PANEL,
        borderRadius: 12,
        padding: 18,
        marginBottom: 10,
      }}
    >
      <View style={{ flex: 1 }}>
        <Text style={{ color: INK, fontSize: 16, lineHeight: 22 }}>{label}</Text>
        <Text style={{ color: MUTED, fontSize: 13, lineHeight: 18 }}>{detail}</Text>
      </View>
      <Switch
        value={value}
        onValueChange={onValueChange}
        accessibilityLabel={label}
        trackColor={{ false: '#3a4d63', true: '#2fb5a8' }}
        thumbColor="#f4f8fb"
      />
    </View>
  );
}

export default function SettingsRoute() {
  const toggle = useConformance((state) => state.toggle);
  const setToggle = useConformance((state) => state.setToggle);
  return (
    <Screen>
      <Heading
        title="Settings"
        detail="One switch, shared with the Controls tab through the store."
      />
      <Row
        label="Gallery switch"
        detail="The same value the Switch on the Controls tab shows"
        value={toggle}
        onValueChange={setToggle}
      />
    </Screen>
  );
}
