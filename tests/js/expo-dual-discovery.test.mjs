import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';

const cli = path.resolve('packages/expo-qnx-autolinking/bin/expo-qnx-autolinking.js');
const project = await mkdtemp(path.join(tmpdir(), 'rnqnx-dual-discovery-'));
try {
  await writeFile(path.join(project, 'package.json'), JSON.stringify({
    name: 'dual-discovery', version: '1.0.0', private: true,
    expo: { autolinking: { nativeModulesDir: './modules' } },
  }));
  for (const name of ['shared', 'qnx-only', 'marathon-only', 'apple-only']) {
    const dir = path.join(project, 'modules', name);
    await mkdir(dir, { recursive: true });
    await writeFile(path.join(dir, 'package.json'), JSON.stringify({ name, version: '1.0.0' }));
    const platforms = name === 'shared' ? ['qnx', 'marathon'] : [name.replace('-only', '')];
    /** @type {Record<string, unknown>} */
    const config = { platforms };
    for (const platform of platforms) {
      const target = `${name.replaceAll('-', '_')}_${platform}`;
      config[platform] = { target, modules: ['Counter'], cmakeListsPath: `${platform}/CMakeLists.txt` };
      await mkdir(path.join(dir, platform));
      await writeFile(path.join(dir, platform, 'CMakeLists.txt'), `add_library(${target} INTERFACE)\n`);
    }
    await writeFile(path.join(dir, 'expo-module.config.json'), JSON.stringify(config));
  }
  for (const platform of ['qnx', 'marathon']) {
    const json = path.join(project, `${platform}.json`);
    const cmake = path.join(project, `${platform}.cmake`);
    const generated = spawnSync(process.execPath, [
      cli, '--project-root', project, '--platform', platform, '--json', json, '--cmake', cmake,
    ], { encoding: 'utf8', timeout: 30000, maxBuffer: 1024 * 1024 });
    assert.equal(generated.error, undefined);
    assert.equal(generated.status, 0, generated.stderr);
    const report = JSON.parse(await readFile(json, 'utf8'));
    assert.equal(report.platform, platform);
    assert.deepEqual(report.modules.map((/** @type {{packageName: string}} */ item) => item.packageName).sort(), [`${platform}-only`, 'shared'].sort());
    const consumer = path.join(project, `consumer-${platform}`);
    await mkdir(consumer);
    await writeFile(path.join(consumer, 'CMakeLists.txt'), [
      'cmake_minimum_required(VERSION 3.16)', 'project(DualDiscovery NONE)',
      `include("${cmake}")`,
      `if(NOT TARGET shared_${platform})`, '  message(FATAL_ERROR "Missing shared target")', 'endif()',
      `list(LENGTH RNQNX_EXPO_${platform.toUpperCase()}_MODULE_TARGETS count)`,
      'if(NOT count EQUAL 2)', '  message(FATAL_ERROR "Wrong target count")', 'endif()', '',
    ].join('\n'));
    const configured = spawnSync('cmake', ['-S', consumer, '-B', path.join(project, `build-${platform}`)], {
      encoding: 'utf8', timeout: 30000, maxBuffer: 1024 * 1024,
    });
    assert.equal(configured.error, undefined);
    assert.equal(configured.status, 0, configured.stderr);
    console.log(`real Expo ${platform} discovery and CMake INTERFACE linkage passed; no native runtime claim`);
  }
} finally {
  await rm(project, { recursive: true, force: true });
}
