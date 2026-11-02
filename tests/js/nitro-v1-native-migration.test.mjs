// Static source-level contract; actual compilation/target proof remains required.
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
/** @param {string} p */
const read = (p) => readFile(p, 'utf8');
const root = JSON.parse(await read('package.json'));
const manifest = JSON.parse(await read('packages/qnx-nitro-runtime/package.json'));
const version = '1.0.0-alpha.nightly.105.g69d365fc9b18';
assert.equal(root.devDependencies['react-native-nitro-modules'], version);
assert.equal(root.devDependencies.nitrogen, version);
assert.equal(root.devDependencies['nitro-core'], version);
assert.equal(manifest.peerDependencies['nitro-core'], version);
const c = await read('packages/qnx-nitro-runtime/CMakeLists.txt');
const h = await read('packages/qnx-nitro-runtime/cpp/QnxPlatformInfo.hpp');
const src = await read('packages/qnx-nitro-runtime/cpp/QnxPlatformInfo.cpp');
const js = await read('packages/qnx-nitro-runtime/index.js');
const p = await read('vnext/probes/runtime-qnx/main.cpp');
assert.match(c, /native\/cpp\/src/);
assert.match(c, /NitroModules\/HybridClassRegistry.hpp/);
assert.match(h, /struct Class/);
assert.match(src, /registerHybridConstructor/);
assert.match(src, /registerHybridProperty/);
assert.match(js, /require\('nitro-core'\)/);
assert.match(js, /getHybridClass\('QnxPlatformInfo'\)/);
assert.match(p, /NitroModulesProxy\.getHybridClass/);
console.log('nitro-v1-native-migration.test.mjs passed (source only)');
