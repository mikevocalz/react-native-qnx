// Reviewed source baseline, not an assertion of the latest npm versions.
// Keep this aligned with the platform's compatibility change and frozen graph.
export const CONSUMER_CONTRACT = Object.freeze({
  schemaVersion: 1,
  baseline: '43f0692aabea57e0f965b892858811349db8ec5a',
  minimumNodeMajor: 26,
  packageManager: 'pnpm@12.18.0',
  packages: Object.freeze({
    react: '19.3.0',
    'react-native': '0.88.1',
    expo: '58.0.24',
    metro: '0.87.1',
    'babel-preset-expo': '58.0.25',
    '@react-native/metro-config': '0.88.1',
    '@rn-qnx/react-native-qnx': '0.1.0',
    '@rn-qnx/metro-config-qnx': '0.1.0',
  }),
  // The drawer and tabs starter (qnx-only projects) adds these.
  routerPackages: Object.freeze({
    '@rn-qnx/expo-router-qnx': '0.1.0',
    'expo-router': '58.0.31',
    'expo-linking': '58.0.17',
    '@expo/metro-runtime': '58.0.17',
    'expo-status-bar': '58.0.5',
    'react-native-safe-area-context': '5.11.1',
    'react-native-screens': '4.30.1',
    'react-native-drawer-layout': '4.2.12',
    'react-dom': '19.3.0',
  }),
  marathonPackages: Object.freeze({
    '@rn-qnx/react-native-marathon': '0.1.0',
    '@rn-qnx/metro-config-marathon': '0.1.0',
  }),
});
