import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdir, mkdtemp, readFile, rename, rm, symlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { test } from 'node:test';
import { discoverNativePackages } from '../../packages/react-native-qnx-cli/src/native-packages.mjs';
import { prepareNativeRuntime } from '../../packages/react-native-qnx-cli/src/build-native.mjs';

const root = path.resolve(import.meta.dirname, '../..');
const version = '1.0.0-alpha.nightly.105.g69d365fc9b18';
const metadata = {
  schemaVersion: 1,
  platforms: {
    qnx: {
      cmakeListsPath: 'native/CMakeLists.txt',
      target: 'example_native',
      registrationHeader: 'Example/Registration.hpp',
      registrationFunction: 'example::registerClasses',
      nitroCoreVersions: [version],
      requiresDispatcher: true,
    },
  },
};

async function fixture() {
  const directory = await mkdtemp(path.join(tmpdir(), 'rnqnx-native-package-'));
  const installed = path.join(directory, 'node_modules/example-native');
  await mkdir(path.join(installed, 'native'), { recursive: true });
  await writeFile(
    path.join(directory, 'package.json'),
    JSON.stringify({ dependencies: { facade: '1' } })
  );
  await mkdir(path.join(directory, 'node_modules/facade'));
  await writeFile(
    path.join(directory, 'node_modules/facade/package.json'),
    JSON.stringify({
      name: 'facade',
      version: '1',
      dependencies: { 'example-native': '1' },
      optionalDependencies: { 'absent-optional': '1' },
    })
  );
  await writeFile(
    path.join(installed, 'package.json'),
    JSON.stringify({
      name: 'example-native',
      version: '1',
      exports: { '.': './index.js' },
      rnqnxNative: metadata,
    })
  );
  await writeFile(
    path.join(installed, 'native/CMakeLists.txt'),
    'add_library(example_native INTERFACE)\n'
  );
  return { directory, installed };
}

test('discovers transitive selected packages, ignores other platforms, and supports pnpm symlinks', async () => {
  const { directory, installed } = await fixture();
  try {
    const entries = await discoverNativePackages(directory, 'qnx');
    assert.equal(entries.length, 1);
    assert.equal(entries[0].packageRoot, installed);
    assert.equal(entries[0].registrationFunction, 'example::registerClasses');
    assert.match(entries[0].manifestSha256, /^[a-f0-9]{64}$/);
    assert.deepEqual(await discoverNativePackages(directory, 'marathon'), []);
    const store = path.join(directory, 'node_modules/.pnpm/example-native');
    await mkdir(path.dirname(store), { recursive: true });
    await rename(installed, store);
    await symlink(store, installed);
    assert.equal((await discoverNativePackages(directory, 'qnx'))[0].packageRoot, store);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test('does not infer native support from an Expo config or a package name', async () => {
  const { directory, installed } = await fixture();
  try {
    await writeFile(
      path.join(installed, 'package.json'),
      JSON.stringify({ name: 'example-native', version: '1' })
    );
    await writeFile(
      path.join(installed, 'expo-module.config.json'),
      JSON.stringify({ platforms: ['qnx'] })
    );
    assert.deepEqual(await discoverNativePackages(directory, 'qnx'), []);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test('rejects escaped source paths, registration injection and missing required dependencies', async () => {
  const { directory, installed } = await fixture();
  try {
    for (const change of [
      { cmakeListsPath: '../CMakeLists.txt' },
      { registrationHeader: 'x>\n#error unexpected' },
      { registrationFunction: 'example::run();evil' },
      { target: 'a;message' },
      { target: undefined },
      { registrationFunction: null },
      { nitroCoreVersions: [] },
      { requiresDispatcher: 'yes' },
    ]) {
      await writeFile(
        path.join(installed, 'package.json'),
        JSON.stringify({
          name: 'example-native',
          version: '1',
          rnqnxNative: {
            ...metadata,
            platforms: { qnx: { ...metadata.platforms.qnx, ...change } },
          },
        })
      );
      await assert.rejects(() => discoverNativePackages(directory, 'qnx'));
    }
    for (const change of [{ name: undefined }, { name: null }, { version: '' }]) {
      await writeFile(
        path.join(installed, 'package.json'),
        JSON.stringify({
          name: 'example-native',
          version: '1',
          rnqnxNative: metadata,
          ...change,
        })
      );
      await assert.rejects(() => discoverNativePackages(directory, 'qnx'), /identity/);
    }
    await rm(path.join(installed, 'package.json'));
    await assert.rejects(() => discoverNativePackages(directory, 'qnx'), /Install dependency/);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test('native CMake requires matching version, dispatcher and unchanged installed manifest', async () => {
  const { directory } = await fixture();
  try {
    const entries = await discoverNativePackages(directory, 'qnx');
    const configuration = { nativePackages: entries };
    const source = path.join(directory, 'consumer');
    await mkdir(source);
    await writeFile(
      path.join(source, 'main.cpp'),
      '#include RNQNX_NATIVE_PACKAGES_HEADER\nint main() { return 0; }\n'
    );
    await writeFile(path.join(source, 'native-app.json'), JSON.stringify(configuration));
    await writeFile(
      path.join(source, 'CMakeLists.txt'),
      `cmake_minimum_required(VERSION 3.22)
project(RegistrationContract LANGUAGES CXX)
add_library(rnqnx_nitro_core INTERFACE)
set_target_properties(rnqnx_nitro_core PROPERTIES NITRO_CORE_VERSION "\${TEST_VERSION}")
if(TEST_DISPATCHER)
  add_library(rnqnx_nitro_qt INTERFACE)
endif()
add_executable(consumer main.cpp)
file(READ "\${CMAKE_CURRENT_SOURCE_DIR}/native-app.json" config)
include("${root}/cmake/RnqnxNativePackages.cmake")
rnqnx_link_native_packages(consumer "\${config}")
`
    );
    /** @param {string} name @param {boolean} accepted @param {string} suppliedVersion @param {boolean} dispatcher */
    function configure(name, accepted, suppliedVersion = version, dispatcher = true) {
      const result = spawnSync(
        process.env.CMAKE ?? 'cmake',
        [
          '-S',
          source,
          '-B',
          path.join(directory, name),
          `-DTEST_VERSION=${suppliedVersion}`,
          `-DTEST_DISPATCHER=${dispatcher ? 'ON' : 'OFF'}`,
        ],
        { encoding: 'utf8' }
      );
      assert.equal(result.error, undefined);
      assert.equal(result.status === 0, accepted, result.stdout + result.stderr);
      return result;
    }
    configure('valid', true);
    const header = await readFile(
      path.join(directory, 'valid/consumer-NativePackages.generated.h'),
      'utf8'
    );
    assert.match(header, /example::registerClasses\(\)/);
    assert.match(configure('wrong-version', false, 'different').stderr, /does not support/);
    assert.match(configure('no-dispatcher', false, version, false).stderr, /requires the real/);
    await writeFile(path.join(entries[0].packageRoot, 'package.json'), '{}');
    assert.match(configure('changed-package', false).stderr, /changed/);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});

test('runtime copy rejects symlinked build parents and reused patch targets outside the copy', async () => {
  const directory = await mkdtemp(path.join(tmpdir(), 'rnqnx-native-copy-'));
  try {
    const source = path.join(directory, 'installed/nitro-core');
    const headers = path.join(source, 'native/cpp/include/NitroModules');
    await mkdir(headers, { recursive: true });
    for (const header of ['Dispatcher.hpp', 'Callback.hpp', 'NitroDefines.hpp'])
      await writeFile(path.join(headers, header), 'original installed source\n');
    const build = path.join(directory, 'build');
    await mkdir(build);
    const runtime = {
      source,
      version,
      manifestSha256: 'fixture',
      destination: path.join(build, 'native-dependencies/nitro-core'),
    };
    await symlink(path.dirname(source), path.join(build, 'native-dependencies'));
    await assert.rejects(() => prepareNativeRuntime(runtime), /build-local/);
    await rm(path.join(build, 'native-dependencies'));
    await prepareNativeRuntime(runtime);
    await prepareNativeRuntime(runtime);
    const copied = path.join(runtime.destination, 'native/cpp/include/NitroModules/Dispatcher.hpp');
    assert.equal(await readFile(copied, 'utf8'), 'original installed source\n');
    await rm(copied);
    await symlink(path.join(headers, 'Dispatcher.hpp'), copied);
    await assert.rejects(() => prepareNativeRuntime(runtime), /symlink/);
    assert.equal(
      await readFile(path.join(headers, 'Dispatcher.hpp'), 'utf8'),
      'original installed source\n'
    );
    await rm(copied);
    await writeFile(copied, 'owned copied source\n');
    const qnxHeader = path.join(
      runtime.destination,
      'native/cpp/include/NitroModules/NitroDefines.hpp'
    );
    await rm(qnxHeader);
    await symlink(path.join(headers, 'NitroDefines.hpp'), qnxHeader);
    await assert.rejects(() => prepareNativeRuntime(runtime), /symlink/);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
});
