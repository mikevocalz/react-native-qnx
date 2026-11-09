#!/usr/bin/env node
// Nitro v1 alpha source, install, ABI, runtime-probe contract gate.
// The QNX target is not considered proven until a licensed SDP cross-build
// and a real QNX VM/Pi execution succeed.
import assert from 'node:assert/strict';
import { readFile, access } from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';

const root = process.cwd();
const require = createRequire(import.meta.url);
const expected = '1.0.0-alpha.nightly.105.g69d365fc9b18';
/** @param {string} p */
const read = (p) => readFile(p, 'utf8');
const coreManifestPath = require.resolve('nitro-core/package.json');
const nitroManifestPath = require.resolve('react-native-nitro-modules/package.json');
const nitrogenManifestPath = require.resolve('nitrogen/package.json');
const core = JSON.parse(await read(coreManifestPath));
const nitro = JSON.parse(await read(nitroManifestPath));
const nitrogen = JSON.parse(await read(nitrogenManifestPath));

for (const [name, manifest] of [
  ['nitro-core', core],
  ['react-native-nitro-modules', nitro],
  ['nitrogen', nitrogen],
]) {
  assert.equal(manifest.version, expected, `${name} must be pinned to exact same Nitro alpha`);
}
assert.equal(nitro.dependencies['nitro-core'], expected);
assert.equal(nitrogen.dependencies['nitro-core'], expected);

const base = path.join(path.dirname(coreManifestPath), 'native/cpp');
for (const p of [
  'include/NitroModules/HybridClassRegistry.hpp',
  'include/NitroModules/HybridClass.hpp',
  'include/NitroModules/HybridObject.hpp',
  'include/NitroModules/InstallNitro.hpp',
  'src/HybridClassRegistry.cpp',
  'src/HybridObject.cpp',
  'src/InstallNitro.cpp',
])
  await access(path.join(base, p));
const reg = await read(path.join(base, 'include/NitroModules/HybridClassRegistry.hpp'));
const hybr = await read(path.join(base, 'include/NitroModules/HybridClass.hpp'));
assert.match(reg, /registerHybridClass/);
assert.match(hybr, /registerHybridConstructor/);
assert.match(hybr, /registerHybridProperty/);
assert.match(hybr, /registerHybridMethod/);
const logger = await read(path.join(root, 'packages/qnx-nitro-runtime/cpp/NitroLogger.cpp'));
const thread = await read(path.join(root, 'packages/qnx-nitro-runtime/cpp/ThreadUtils.cpp'));
assert.ok(logger.includes('#include <NitroModules/NitroLogger.hpp>'));
assert.ok(thread.includes('#include <NitroModules/ThreadUtils.hpp>'));
const runtime = await read(path.join(root, 'packages/qnx-nitro-runtime/cpp/QnxNitroRuntime.cpp'));
const klass = await read(
  path.join(root, 'packages/qnx-nitro-runtime/cpp/generated/QnxPlatformInfo.binding.cpp')
);
const cmake = await read(path.join(root, 'packages/qnx-nitro-runtime/CMakeLists.txt'));
const sharedCmake = await read(path.join(root, 'cmake/RnqnxNitroV1.cmake'));
const probe = await read(path.join(root, 'vnext/probes/runtime-qnx/main.cpp'));
const probeFixture = await read(path.join(root, 'vnext/probes/runtime-qnx/nitro.js'));
assert.match(runtime, /HybridClassRegistry::registerHybridClass/);
assert.match(runtime, /margelo::nitro::install\(runtime\)/);
assert.doesNotMatch(runtime, /HybridObjectRegistry|install\(runtime,/);
assert.match(klass, /HybridClass::create<Class>/);
assert.match(cmake, /RnqnxNitroV1\.cmake/);
assert.match(cmake, /rnqnx_configure_nitro_v1_adapter/);
assert.match(sharedCmake, /nitro-core/);
assert.match(sharedCmake, /GlobalErrorHandler\.cpp/);
assert.match(probe, /rnqnx::nitro::installQnxNitroRuntime\(\*runtime\)/);
assert.match(probeFixture, /NitroModulesProxy\.getHybridClass\('QnxPlatformInfo'\)/);
assert.doesNotMatch(probeFixture, /NitroModulesProxy\.createHybridObject/);
console.log('Nitro v1 source/ABI contracts found; actual QNX native execution NOT yet proven.');
