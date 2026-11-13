// Expo native modules on QNX: expo-constants reads its ExponentConstants
// module through expo-modules-core's requireNativeModule, the same call it
// makes on iOS and Android. The values come from the QNX implementation in
// @rn-qnx/expo-constants-qnx (uname, the embedded app config).
import * as React from 'react';
import { Text, View } from 'react-native';
import Constants from 'expo-constants';

const INK = '#e9f0f7';
const MUTED = '#9dafc2';
const ACCENT = '#79e8dc';

function Row({ name, value }) {
  return (
    <View style={{ flexDirection: 'row', marginBottom: 4 }}>
      <Text style={{ color: MUTED, fontSize: 14, lineHeight: 20, width: 150 }}>{name}</Text>
      <Text style={{ color: INK, fontSize: 14, lineHeight: 20, flex: 1 }} numberOfLines={1}>
        {value === undefined || value === null || value === '' ? '—' : String(value)}
      </Text>
    </View>
  );
}

export default function ExpoModulesCard() {
  const qnx = Constants.platform?.qnx;
  const modules = Object.keys(globalThis.expo?.modules ?? {}).sort();
  return (
    <View>
      <Text style={{ color: ACCENT, fontSize: 16, lineHeight: 22, marginBottom: 10 }}>
        {modules.length
          ? 'globalThis.expo.modules: ' + modules.join(', ')
          : 'No Expo native modules registered'}
      </Text>
      <Row name="expoConfig.name" value={Constants.expoConfig?.name} />
      <Row name="expoConfig.slug" value={Constants.expoConfig?.slug} />
      <Row name="deviceName" value={Constants.deviceName} />
      <Row
        name="system"
        value={qnx ? `${qnx.systemName} ${qnx.systemVersion} ${qnx.machine}` : null}
      />
      <Row name="environment" value={Constants.executionEnvironment} />
      <Row name="debugMode" value={Constants.debugMode} />
      <Row name="sessionId" value={Constants.sessionId} />
    </View>
  );
}
