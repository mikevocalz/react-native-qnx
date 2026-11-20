import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

// The React Native version the C++ host reports (Platform.constants
// reactNativeVersion) comes from the root package.json pin through
// cmake/RnqnxReactQt.cmake. No C++ source may spell the version out again.
const host = await readFile('vnext/src/runtime/QtReactHost.cpp', 'utf8');
const cmake = await readFile('cmake/RnqnxReactQt.cmake', 'utf8');

for (const macro of [
  'RNQNX_REACT_NATIVE_VERSION_MAJOR',
  'RNQNX_REACT_NATIVE_VERSION_MINOR',
  'RNQNX_REACT_NATIVE_VERSION_PATCH',
  'RNQNX_REACT_NATIVE_VERSION_PRERELEASE',
]) {
  assert.ok(host.includes(macro), `QtReactHost.cpp must read ${macro}`);
  assert.ok(cmake.includes(macro), `RnqnxReactQt.cmake must define ${macro}`);
}
assert.match(cmake, /devDependencies react-native/, 'CMake must read the react-native pin from package.json');

const body = host.slice(host.indexOf('QtReactHost::installPlatformConstants'));
const installer = body.slice(0, body.indexOf('\n}\n'));
assert.doesNotMatch(installer, /"major",\s*\d/, 'major version must not be a literal');
assert.doesNotMatch(installer, /"minor",\s*\d/, 'minor version must not be a literal');
assert.doesNotMatch(installer, /"patch",\s*\d/, 'patch version must not be a literal');
assert.doesNotMatch(installer, /"rc\.\d+"/, 'prerelease must not be a literal');
