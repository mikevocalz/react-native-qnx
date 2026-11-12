import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

/** @param {string} p */
const read = async (p) => JSON.parse(await readFile(p, 'utf8'));
const root = await read('package.json');
assert.equal(root.devDependencies.expo, '58.0.24');
assert.equal(root.devDependencies['react-native'], '0.88.1');
assert.equal(root.devDependencies.react, '19.3.0');
assert.equal(root.devDependencies.metro, '0.87.1');
assert.equal(root.devDependencies['@react-native/metro-config'], '0.88.1');
assert.equal(root.devDependencies['babel-preset-expo'], '58.0.25');

for (const file of [
  'apps/rn-playground/package.json',
  'packages/react-native-qnx/package.json',
  'packages/metro-config-qnx/package.json',
  'packages/qnx-nitro-runtime/package.json',
  'packages/expo-modules-core-qnx/package.json',
  'packages/expo-qnx-autolinking/package.json',
  'packages/expo-qnx-config-plugins/package.json',
]) {
  const pkg = await read(file);
  for (const [name, value] of Object.entries({
    ...pkg.dependencies,
    ...pkg.peerDependencies,
  })) {
    if (name in root.devDependencies && !name.startsWith('@rn-qnx/')) {
      assert.equal(value, root.devDependencies[name], `${file} ${name} differs from workspace pin`);
    }
  }
}
const expo = await read('packages/expo-modules-core-qnx/package.json');
assert.equal(expo.peerDependencies['expo-modules-core'], '58.1.0');
console.log('sdk58-stable-manifest-pins.test.mjs passed');
