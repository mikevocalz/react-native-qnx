import * as React from 'react';
import { Text, View } from 'react-native';
import Constants from 'expo-constants';
import { Screen, Heading, ACCENT, INK, MUTED, PANEL } from '../ui';

function Line({ name, value }) {
  return (
    <View style={{ flexDirection: 'row', marginBottom: 6 }}>
      <Text style={{ color: MUTED, fontSize: 14, lineHeight: 20, width: 150 }}>{name}</Text>
      <Text style={{ color: INK, fontSize: 14, lineHeight: 20, flex: 1 }}>
        {String(value ?? '')}
      </Text>
    </View>
  );
}

export default function AboutRoute() {
  const qnx = Constants.platform?.qnx;
  return (
    <Screen>
      <Heading
        title="About"
        detail="React Native on QNX, rendered by Fabric and Qt Quick, routed by expo-router."
      />
      <View style={{ backgroundColor: PANEL, borderRadius: 14, padding: 22 }}>
        <Text style={{ color: ACCENT, fontSize: 14, marginBottom: 10 }}>
          {__DEV__ ? 'Development client' : 'Release build'}
        </Text>
        <Line name="App" value={Constants.expoConfig?.name} />
        <Line name="Slug" value={Constants.expoConfig?.slug} />
        <Line
          name="System"
          value={qnx ? `${qnx.systemName} ${qnx.systemVersion} ${qnx.machine}` : 'unknown'}
        />
      </View>
    </Screen>
  );
}
