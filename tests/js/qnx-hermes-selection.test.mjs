import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtemp, mkdir, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';

// Exercise the production CMake importer with synthetic files. This verifies
// artifact selection and rejection, not Hermes ABI or QNX execution.
const temp = await mkdtemp(path.join(os.tmpdir(), 'rnqnx-hermes-selection-'));
try {
  const source = path.join(temp, 'hermes');
  const artifacts = path.join(temp, 'artifacts');
  for (const dir of [source, path.join(artifacts, 'lib'), path.join(artifacts, 'jsi')]) {
    await mkdir(dir, { recursive: true });
  }
  await writeFile(path.join(source, '.rnqnx-source-tag'), 'hermes-v260318099.0.5\n');
  for (const file of ['lib/libhermesvm.so', 'lib/libhermesvmlean.so', 'jsi/libjsi.a']) {
    await writeFile(path.join(artifacts, file), 'synthetic configuration fixture\n');
  }
  const importer = path.resolve('cmake/dependencies/HermesQNX.cmake');
  await writeFile(
    path.join(temp, 'CMakeLists.txt'),
    `cmake_minimum_required(VERSION 3.24)
project(HermesSelection NONE)
include("${importer}")
get_target_property(links rnqnx_hermes INTERFACE_LINK_LIBRARIES)
file(WRITE "\${CMAKE_BINARY_DIR}/selected.txt" "\${links}")
`
  );
  const cases = [
    { name: 'release', dev: false, worklets: false, library: 'libhermesvmlean.so' },
    { name: 'development', dev: true, worklets: false, library: 'libhermesvm.so' },
    { name: 'worklets-release', dev: false, worklets: true, library: 'libhermesvm.so' },
    { name: 'worklets-development', dev: true, worklets: true, library: 'libhermesvm.so' },
  ];
  for (const entry of cases) {
    const build = path.join(temp, entry.name);
    const result = spawnSync(
      'cmake',
      [
        '-S',
        temp,
        '-B',
        build,
        '-DCMAKE_SYSTEM_NAME=QNX',
        `-DRNQNX_HERMES_QNX_SOURCE_DIR=${source}`,
        `-DRNQNX_HERMES_QNX_DIR=${artifacts}`,
        `-DRNQNX_ENABLE_DEV_SUPPORT=${entry.dev ? 'ON' : 'OFF'}`,
        `-DRNQNX_ENABLE_WORKLETS=${entry.worklets ? 'ON' : 'OFF'}`,
      ],
      { encoding: 'utf8' }
    );
    assert.equal(result.status, 0, result.stdout + result.stderr);
    assert.equal(
      await readFile(path.join(build, 'selected.txt'), 'utf8'),
      `${path.join(artifacts, 'lib', entry.library)};jsi`
    );
  }
  await rm(path.join(artifacts, 'lib/libhermesvm.so'));
  const missing = spawnSync(
    'cmake',
    [
      '-S',
      temp,
      '-B',
      path.join(temp, 'missing-full'),
      '-DCMAKE_SYSTEM_NAME=QNX',
      `-DRNQNX_HERMES_QNX_SOURCE_DIR=${source}`,
      `-DRNQNX_HERMES_QNX_DIR=${artifacts}`,
      '-DRNQNX_ENABLE_DEV_SUPPORT=OFF',
      '-DRNQNX_ENABLE_WORKLETS=ON',
    ],
    { encoding: 'utf8' }
  );
  assert.notEqual(missing.status, 0);
  assert.match(missing.stdout + missing.stderr, /Missing QNX Hermes artifact:.*libhermesvm\.so/s);
  console.log(
    'QNX Hermes CMake selection: release lean, development/worklets full, missing full rejected'
  );
} finally {
  await rm(temp, { recursive: true, force: true });
}
