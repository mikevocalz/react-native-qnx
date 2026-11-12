import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { SUPPORT_FILE, classifyPackage, driftProblems } from '../../scripts/expo-sdk58-support.mjs';

// Offline: the committed list must match the installed expo's SDK package set,
// and the classifier must keep its rules. Regenerating needs the network.
const committed = JSON.parse(await readFile(SUPPORT_FILE, 'utf8'));
const expoManifest = JSON.parse(await readFile('node_modules/expo/package.json', 'utf8'));
const bundled = JSON.parse(await readFile('node_modules/expo/bundledNativeModules.json', 'utf8'));
let checks = 0;
const ok = (/** @type {unknown} */ v, /** @type {string} */ m) => {
  assert.ok(v, m);
  checks++;
};
assert.deepEqual(driftProblems(committed, { expoVersion: expoManifest.version, bundled }), []);
checks++;

// Drift is detected: version bump, range change, added and removed packages.
ok(
  driftProblems(committed, { expoVersion: '58.0.99', bundled }).some((p) =>
    /installed is 58\.0\.99/.test(p)
  ),
  'expo bump'
);
const changed = { ...bundled, 'expo-haptics': '~99.0.0', 'expo-new-thing': '~1.0.0' };
delete changed['expo-sms'];
const problems = driftProblems(committed, { expoVersion: expoManifest.version, bundled: changed });
ok(
  problems.some((p) => p.startsWith('expo-haptics range')),
  'range change'
);
ok(
  problems.some((p) => p.startsWith('expo-new-thing is in the SDK')),
  'added package'
);
ok(
  problems.some((p) => p.startsWith('expo-sms is listed')),
  'removed package'
);
const tampered = structuredClone(committed);
tampered.packages.find((/** @type {any} */ p) => p.name === 'expo-haptics').status = 'qnx-native';
ok(
  driftProblems(tampered, { expoVersion: expoManifest.version, bundled }).some((p) =>
    p.startsWith('expo-haptics is qnx-native')
  ),
  'hand edit'
);

// Classifier rules.
const c = (/** @type {string[]} */ files, /** @type {any} */ moduleConfig = null) =>
  classifyPackage('example', { files, moduleConfig }).status;
ok(c(['/ios/Foo.swift']) === 'unsupported', 'ios code');
ok(c(['/android/build.gradle']) === 'unsupported', 'android code');
ok(c(['/expo-module.config.json'], { platforms: ['qnx', 'ios'] }) === 'qnx-native', 'qnx declared');
ok(c(['/expo-module.config.json'], { platforms: ['web'] }) === 'no-op', 'web only');
ok(c([]) === 'js-only', 'no native files');
ok(
  classifyPackage('react-native', { files: ['/ios/x'], moduleConfig: null }).status === 'platform',
  'override'
);
ok(committed.packages.length === Object.keys(bundled).length, 'every SDK package listed');
ok(
  committed.packages.every((/** @type {any} */ p) => p.reason && p.resolved),
  'every row has a reason and version'
);
console.log(
  `Expo SDK 58 support list: ${checks} checks passed (${JSON.stringify(committed.counts)}).`
);
