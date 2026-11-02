import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

/** @param {string} file */
const read = (file) => readFile(file, 'utf8');
const cmake = await read('CMakeLists.txt');
const native = await read('cmake/RnqnxMarathonHost.cmake');
const host = await read('vnext/probes/playground/main.cpp');
assert.match(cmake, /option\(RNQNX_BUILD_MARATHON_HOST[\s\S]*?OFF\)/);
assert.match(cmake, /CMAKE_SYSTEM_NAME STREQUAL "Linux"/);
assert.match(cmake, /if\(RNQNX_BUILD_REACT_QNX\)/);
assert.match(native, /target_link_libraries\(rnqnx_marathon_host PRIVATE rnqnx_react_host\)/);
assert.match(native, /--platform marathon --dev false/);
assert.match(native, /metro\.marathon\.config\.js/);
assert.match(
  native,
  /DEPENDS[\s\S]*?apps\/rn-playground\/qnx-asset\.png/,
  'Metro must rebuild the Marathon bundle when qnx-asset.png changes'
);
assert.match(native, /NOT TARGET rnqnx_react_host/);
// Only inspect active CMake statements. Comments must be allowed to explain
// why native MarathonOS builds do NOT link QNX Screen or SDP libraries.
const activeCmake = native
  .split('\n')
  .filter((line) => !line.trimStart().startsWith('#'))
  .join('\n');
assert.doesNotMatch(activeCmake, /libscreen|qnx_platform|QNX_SDP_ENV/);
assert.match(host, /#ifdef RNQNX_MARATHON_HOST/);
assert.match(host, /org\.rnqnx\.marathon\.playground/);
assert.match(host, /setDesktopFileName/);
console.log('marathon-native-host-contract.test.mjs passed');
