import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import fs from 'node:fs';

const require = createRequire(import.meta.url);
const expoRequire = createRequire(require.resolve('expo/package.json'));
const metadata = JSON.parse(
  fs.readFileSync(expoRequire.resolve('expo-modules-autolinking/package.json'), 'utf8')
);
assert.equal(metadata.version, '58.1.0', 're-audit the SDK integration before moving this patch');
const { getSupportPackageForPlatform, getLinkingImplementationForPlatform } = expoRequire(
  'expo/internal/unstable-autolinking-exports'
);
assert.equal(getSupportPackageForPlatform('qnx'), '@rn-qnx/react-native-qnx');
assert.equal(getSupportPackageForPlatform('ios'), 'react-native');
assert.equal(getSupportPackageForPlatform('android'), 'react-native');
assert.equal(getSupportPackageForPlatform('windows'), 'react-native-windows');
assert.equal(getSupportPackageForPlatform('macos'), 'react-native-macos');
assert.equal(getSupportPackageForPlatform('web'), null);
assert.equal(getSupportPackageForPlatform('devtools'), null);
assert.throws(
  () => getSupportPackageForPlatform('unrecognized-platform'),
  /No support package is known/
);
assert.throws(
  () => getLinkingImplementationForPlatform('qnx'),
  /No linking implementation is available/,
  'host identity must not silently manufacture QNX native autolinking'
);
console.log(
  'Expo host-package lookup: actual installed QNX identity, existing platforms and fail-closed native linking pass.'
);
