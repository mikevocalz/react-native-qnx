import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdir, mkdtemp, readFile, realpath, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';

// Unlike the preparation unit tests, this test requires the installed, pinned
// Expo discovery implementation. Missing dependencies are failures, not skips.
const cli = path.resolve('packages/expo-qnx/src/cli.mjs');
const project = await realpath(await mkdtemp(path.join(tmpdir(), 'rnqnx-expo-discovery-')));
/** @param {string[]} args */
function run(args) {
  const result = spawnSync(process.execPath, [cli, ...args, '--project', project], {
    encoding: 'utf8', timeout: 30000, killSignal: 'SIGKILL', maxBuffer: 1024 * 1024,
  });
  assert.equal(result.error, undefined);
  assert.equal(result.status, 0, result.stderr);
  return JSON.parse(result.stdout);
}
try {
  await writeFile(path.join(project, 'package.json'), JSON.stringify({
    name: 'rnqnx-preparation-integration', version: '1.0.0', private: true,
    expo: { autolinking: { nativeModulesDir: './modules' } },
  }));
  for (const name of ['counter', 'apple-only']) {
    const dir = path.join(project, 'modules', name);
    await mkdir(dir, { recursive: true });
    await writeFile(path.join(dir, 'package.json'), JSON.stringify({
      name: `@rnqnx-test/${name}`, version: '1.0.0',
    }));
    const config = name === 'counter'
      ? { platforms: ['qnx'], qnx: { target: 'rnqnx_fixture_counter', modules: ['Counter'] } }
      : { platforms: ['apple'], apple: { modules: ['AppleOnly'] } };
    await writeFile(path.join(dir, 'expo-module.config.json'), JSON.stringify(config));
    if (name === 'counter') {
      await writeFile(path.join(dir, 'CMakeLists.txt'),
        'add_library(rnqnx_fixture_counter INTERFACE)\n');
    }
  }
  const inspected = run(['inspect']);
  assert.equal(inspected.modules.length, 1);
  assert.equal(inspected.modules[0].packageName, '@rnqnx-test/counter');
  assert.equal(inspected.runtimeVerified, false);
  const prepared = run(['prepare']);
  assert.equal(prepared.action, 'created');
  assert.equal(prepared.moduleCount, 1);
  assert.equal(run(['prepare', '--check']).action, 'unchanged');
  const generated = path.join(project, 'build/expo-qnx/modules.cmake');
  const text = await readFile(generated, 'utf8');
  assert.match(text, /rnqnx_fixture_counter/);
  assert.doesNotMatch(text, /apple-only|AppleOnly/);

  // Configure the generated file with real CMake and a non-native INTERFACE
  // target. This proves syntax/target wiring, NOT a QNX cross-build or ABI.
  const consumer = path.join(project, 'consumer');
  await mkdir(consumer);
  await writeFile(path.join(consumer, 'CMakeLists.txt'), [
    'cmake_minimum_required(VERSION 3.16)',
    'project(RnqnxPreparedLinkage NONE)',
    `include("${generated.split(path.sep).join('/')}")`,
    'if(NOT TARGET rnqnx_fixture_counter)',
    '  message(FATAL_ERROR "Expected fixture target was not linked")',
    'endif()',
    'if(NOT RNQNX_EXPO_QNX_MODULE_TARGETS STREQUAL "rnqnx_fixture_counter")',
    '  message(FATAL_ERROR "Unexpected generated target list")',
    'endif()',
    '',
  ].join('\n'));
  const configured = spawnSync('cmake', ['-S', consumer, '-B', path.join(project, 'cmake-build')], {
    encoding: 'utf8', timeout: 30000, killSignal: 'SIGKILL', maxBuffer: 1024 * 1024,
  });
  assert.equal(configured.error, undefined);
  assert.equal(configured.status, 0, configured.stderr);
  console.log('Expo QNX: real Expo discovery, deterministic preparation and CMake target wiring passed.');
} finally {
  await rm(project, { recursive: true, force: true });
}
