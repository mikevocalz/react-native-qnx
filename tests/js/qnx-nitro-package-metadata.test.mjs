// A Nitro module published outside this repository declares its QNX build in
// its own package.json (docs/adr/0019). This test installs such a package into
// an app that lives outside the workspace, checks discovery, the descriptor,
// the build-native plan and the CMake reader, then compiles and runs the
// generated registration header on the host. It is not a QNX build.
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { copyFile, mkdir, mkdtemp, readFile, rm, symlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import {
  discoverNitroModules,
  validateNitroDeclaration,
} from '../../packages/react-native-qnx-cli/src/nitro-modules.mjs';
import {
  nativeConfiguration,
  prepareNativeMain,
} from '../../packages/react-native-qnx-cli/src/prepare-native.mjs';
import { planNativeBuild } from '../../packages/react-native-qnx-cli/src/build-native.mjs';
import { inspectConsumer } from '../../packages/react-native-qnx-cli/src/doctor.mjs';
import { useIdentityToolInEnvironment } from './helpers/identity-tool.mjs';

const repo = process.cwd();
const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-nitro-metadata-'));
const project = path.join(temporary, 'apps', 'gauge app');
const store = path.join(temporary, 'store');
// prepare-native always generates the icon set; it finds the tool through
// RNQNX_IDENTITY_TOOL.
const identityTool = await useIdentityToolInEnvironment();
let checks = 0;
/** @param {() => unknown} operation @param {RegExp} pattern */
function throwsWith(operation, pattern) {
  assert.throws(operation, pattern);
  checks++;
}
/** @param {() => Promise<unknown>} operation @param {RegExp} pattern */
async function rejectsWith(operation, pattern) {
  await assert.rejects(operation, pattern);
  checks++;
}

const battery = {
  name: 'react-native-qnx-battery',
  version: '2.1.0',
  'react-native-qnx': {
    nitro: {
      platforms: ['qnx'],
      cmakeListsPath: 'qnx/CMakeLists.txt',
      target: 'QnxBattery',
      initializer: 'margelo::nitro::battery::registerQnxHybridClasses',
    },
  },
};
const gauge = {
  name: '@acme/nitro-gauge',
  version: '0.3.0',
  'react-native-qnx': {
    nitro: {
      platforms: ['qnx'],
      cmakeListsPath: 'CMakeLists.txt',
      target: 'AcmeGauge',
      initializer: 'registerAcmeGauge',
    },
  },
};

/** @param {string} directory @param {object} manifest @param {string} cmakeDir
 * @param {string} target @param {string} source */
async function nitroPackage(directory, manifest, cmakeDir, target, source) {
  await mkdir(path.join(directory, cmakeDir), { recursive: true });
  await writeFile(path.join(directory, 'package.json'), JSON.stringify(manifest, null, 2));
  await writeFile(
    path.join(directory, cmakeDir, 'CMakeLists.txt'),
    `add_library(${target} STATIC module.cpp)\ntarget_compile_features(${target} PRIVATE cxx_std_20)\n`
  );
  await writeFile(path.join(directory, cmakeDir, 'module.cpp'), source);
}

/** @param {string} label @param {string} config @param {string} extra */
function cmakeProject(label, config, extra = '') {
  const source = path.join(temporary, `cmake-${label}`);
  return {
    source,
    async write() {
      await mkdir(source, { recursive: true });
      await writeFile(
        path.join(source, 'CMakeLists.txt'),
        `cmake_minimum_required(VERSION 3.20)
project(ConsumerNitro LANGUAGES CXX)
${extra}
include("${repo}/cmake/RnqnxConsumerApp.cmake")
file(READ "${config}" config)
rnqnx_consumer_nitro_modules("\${config}" "\${CMAKE_BINARY_DIR}" header targets)
add_executable(nitro_probe main.cpp)
target_compile_features(nitro_probe PRIVATE cxx_std_20)
target_link_libraries(nitro_probe PRIVATE \${targets})
target_compile_definitions(nitro_probe PRIVATE RNQNX_CONSUMER_NITRO_HEADER="\${header}")
`
      );
      await writeFile(
        path.join(source, 'main.cpp'),
        '#include RNQNX_CONSUMER_NITRO_HEADER\nint main() { rnqnx::consumer::registerNitroModules(); return 0; }\n'
      );
    },
    /** @param {boolean} good */
    configure(good) {
      const result = spawnSync('cmake', ['-S', source, '-B', path.join(source, 'build')], {
        encoding: 'utf8',
        timeout: 60000,
        maxBuffer: 4 * 1024 * 1024,
      });
      assert.equal(result.error, undefined);
      assert.equal(result.status === 0, good, result.stdout + result.stderr);
      checks++;
      return result.stdout + result.stderr;
    },
  };
}

try {
  // The app sits outside the workspace packages directory. Its node_modules
  // holds one plain copy and one pnpm-style symlink into a store directory.
  await mkdir(path.join(project, 'build/qnx'), { recursive: true });
  await writeFile(
    path.join(project, 'app.json'),
    JSON.stringify({
      expo: {
        name: 'Gauge',
        icon: './icon.png',
        extra: { rnqnx: { appId: 'org.example.gauge', componentName: 'Gauge' } },
      },
    })
  );
  await copyFile(path.join(repo, 'template/qnx/assets/icon.png'), path.join(project, 'icon.png'));
  await writeFile(path.join(project, 'build/qnx/app.hbc'), 'synthetic bytecode\n');
  const appManifest = {
    name: 'gauge-app',
    version: '1.0.0',
    private: true,
    dependencies: {
      'react-native-qnx-battery': '2.1.0',
      '@acme/nitro-gauge': '0.3.0',
      'left-pad': '1.3.0',
    },
  };
  await writeFile(path.join(project, 'package.json'), JSON.stringify(appManifest));
  const modules = path.join(project, 'node_modules');
  await nitroPackage(
    path.join(modules, 'react-native-qnx-battery'),
    battery,
    'qnx',
    'QnxBattery',
    '#include <cstdio>\nnamespace margelo::nitro::battery {\nvoid registerQnxHybridClasses() { std::puts("QNX_BATTERY_REGISTERED"); }\n}\n'
  );
  const gaugeStore = path.join(store, '@acme+nitro-gauge@0.3.0/node_modules/@acme/nitro-gauge');
  await nitroPackage(
    gaugeStore,
    gauge,
    '.',
    'AcmeGauge',
    '#include <cstdio>\nvoid registerAcmeGauge() { std::puts("ACME_GAUGE_REGISTERED"); }\n'
  );
  await mkdir(path.join(modules, '@acme'), { recursive: true });
  await symlink(gaugeStore, path.join(modules, '@acme/nitro-gauge'));
  await mkdir(path.join(modules, 'left-pad'), { recursive: true });
  await writeFile(
    path.join(modules, 'left-pad/package.json'),
    JSON.stringify({ name: 'left-pad', version: '1.3.0' })
  );

  // Discovery reads JSON only and sorts by package name.
  const found = await discoverNitroModules(project, 'qnx');
  assert.deepEqual(
    found.map((module) => [module.package, module.version, module.target, module.initializer]),
    [
      ['@acme/nitro-gauge', '0.3.0', 'AcmeGauge', 'registerAcmeGauge'],
      [
        'react-native-qnx-battery',
        '2.1.0',
        'QnxBattery',
        'margelo::nitro::battery::registerQnxHybridClasses',
      ],
    ]
  );
  checks++;
  assert.equal(found[0].root, gaugeStore.replace(temporary, await realTemporary()));
  assert.equal(found[1].cmakeLists, path.join(found[1].root, 'qnx/CMakeLists.txt'));
  checks += 2;
  assert.deepEqual(await discoverNitroModules(project, 'marathon'), []);
  checks++;

  // The descriptor carries the modules; build-native turns on the runtime.
  const { configuration } = await nativeConfiguration(project, 'qnx', 'build/qnx/app.hbc');
  assert.deepEqual(configuration.nitroModules, found);
  checks++;
  await prepareNativeMain(['--project', project, '--bundle', 'build/qnx/app.hbc']);
  const descriptorFile = path.join(project, 'build/qnx/native-app.json');
  const descriptor = JSON.parse(await readFile(descriptorFile, 'utf8'));
  assert.equal(descriptor.nitroModules.length, 2);
  checks++;
  const savedHost = process.env.QNX_HOST;
  const savedTarget = process.env.QNX_TARGET;
  process.env.QNX_HOST = temporary;
  process.env.QNX_TARGET = temporary;
  try {
    const plan = await planNativeBuild(project, 'qnx');
    assert.ok(plan.configure.includes('-DRNQNX_BUILD_NITRO_QNX=ON'));
    assert.deepEqual(plan.nitroModules, ['@acme/nitro-gauge', 'react-native-qnx-battery']);
    checks += 2;
    if (process.platform === 'linux')
      await rejectsWith(() => planNativeBuild(project, 'qnx', true), /does not build the Nitro/);
    // A Nitro dependency removed after preparation makes the descriptor stale.
    await writeFile(
      path.join(project, 'package.json'),
      JSON.stringify({
        ...appManifest,
        dependencies: { 'react-native-qnx-battery': '2.1.0', 'left-pad': '1.3.0' },
      })
    );
    await rejectsWith(() => planNativeBuild(project, 'qnx'), /stale/);
    await writeFile(path.join(project, 'package.json'), JSON.stringify(appManifest));
  } finally {
    if (savedHost === undefined) delete process.env.QNX_HOST;
    else process.env.QNX_HOST = savedHost;
    if (savedTarget === undefined) delete process.env.QNX_TARGET;
    else process.env.QNX_TARGET = savedTarget;
  }

  // doctor reports the same discovery without running package code.
  const report = await inspectConsumer(project);
  const doctorCheck = report.checks.find((check) => check.id === 'nitro-modules');
  assert.equal(doctorCheck?.status, 'passed');
  assert.equal(doctorCheck?.observed, '@acme/nitro-gauge@0.3.0, react-native-qnx-battery@2.1.0');
  checks += 2;

  // CMake adds each package, links its target and generates the header; the
  // host compiler then builds and runs it.
  const good = cmakeProject('good', descriptorFile, 'add_library(rnqnx_nitro_qnx INTERFACE)');
  await good.write();
  const output = good.configure(true);
  assert.match(output, /Consumer Nitro module: @acme\/nitro-gauge@0\.3\.0 -> AcmeGauge/);
  checks++;
  const header = await readFile(path.join(good.source, 'build/NitroModules.generated.h'), 'utf8');
  assert.match(header, /namespace margelo::nitro::battery \{\nvoid registerQnxHybridClasses\(\);/);
  assert.match(header, /^void registerAcmeGauge\(\);$/m);
  checks += 2;
  const built = spawnSync('cmake', ['--build', path.join(good.source, 'build')], {
    encoding: 'utf8',
    timeout: 120000,
    maxBuffer: 4 * 1024 * 1024,
  });
  assert.equal(built.status, 0, built.stdout + built.stderr);
  const ran = spawnSync(path.join(good.source, 'build/nitro_probe'), { encoding: 'utf8' });
  assert.equal(ran.status, 0);
  assert.equal(ran.stdout, 'ACME_GAUGE_REGISTERED\nQNX_BATTERY_REGISTERED\n');
  checks += 3;

  // CMake rejections: missing runtime, tampered entries.
  const noRuntime = cmakeProject('no-runtime', descriptorFile);
  await noRuntime.write();
  assert.match(noRuntime.configure(false), /RNQNX_BUILD_NITRO_QNX=ON/);
  /** @param {string} label @param {(entry: Record<string, string>) => void} edit @param {RegExp} pattern */
  async function tampered(label, edit, pattern) {
    const copy = structuredClone(descriptor);
    edit(copy.nitroModules[1]);
    const file = path.join(temporary, `${label}.json`);
    await writeFile(file, JSON.stringify(copy));
    const bad = cmakeProject(label, file, 'add_library(rnqnx_nitro_qnx INTERFACE)');
    await bad.write();
    assert.match(bad.configure(false), pattern);
  }
  await tampered('wrong-target', (e) => (e.target = 'NotDefined'), /did not define target/);
  await tampered('reserved-target', (e) => (e.target = 'rnqnx_nitro_qnx'), /invalid CMake target/);
  await tampered('bad-initializer', (e) => (e.initializer = 'f(); evil'), /invalid C\+\+ init/);
  await tampered(
    'outside-root',
    (e) => (e.cmakeLists = path.join(found[0].root, 'CMakeLists.txt')),
    /inside its package root/
  );
  await tampered('extra-key', (e) => (e.extra = 'x'), /exactly package/);

  // Declaration shape rejections.
  const base = battery['react-native-qnx'].nitro;
  for (const [field, pattern] of /** @type {[unknown, RegExp][]} */ ([
    [null, /must be an object/],
    [[], /must be an object/],
    [{ nitro: base, expo: {} }, /expo is not a supported key/],
    [{ nitro: 'x' }, /nitro must be an object/],
    [{ nitro: { ...base, sources: [] } }, /sources is not supported/],
    [{ nitro: { ...base, platforms: [] } }, /nonempty array/],
    [{ nitro: { ...base, platforms: 'qnx' } }, /nonempty array/],
    [{ nitro: { ...base, platforms: ['android'] } }, /supported: qnx/],
    [{ nitro: { ...base, platforms: ['marathon'] } }, /supported: qnx/],
    [{ nitro: { ...base, platforms: ['qnx', 'qnx'] } }, /must not repeat/],
    [{ nitro: { ...base, cmakeListsPath: '../CMakeLists.txt' } }, /package-relative/],
    [{ nitro: { ...base, cmakeListsPath: '/abs/CMakeLists.txt' } }, /package-relative/],
    [{ nitro: { ...base, cmakeListsPath: 'qnx\\CMakeLists.txt' } }, /package-relative/],
    [{ nitro: { ...base, cmakeListsPath: 'qnx/build.cmake' } }, /package-relative/],
    [{ nitro: { ...base, cmakeListsPath: 'q;nx/CMakeLists.txt' } }, /package-relative/],
    [{ nitro: { ...base, target: '' } }, /CMake target name/],
    [{ nitro: { ...base, target: 'has space' } }, /CMake target name/],
    [{ nitro: { ...base, target: 'rnqnx_react_host' } }, /reserved rnqnx_/],
    [{ nitro: { ...base, initializer: '::global' } }, /C\+\+ function/],
    [{ nitro: { ...base, initializer: 'a::b()' } }, /C\+\+ function/],
    [{ nitro: { ...base, initializer: 3 } }, /C\+\+ function/],
    [{ nitro: { platforms: ['qnx'], target: 'T', initializer: 'f' } }, /cmakeListsPath/],
  ]))
    throwsWith(() => validateNitroDeclaration('pkg', field), pattern);

  // Discovery rejections: each names the package at fault.
  const batteryJson = path.join(modules, 'react-native-qnx-battery/package.json');
  /** @param {object} manifest @param {RegExp} pattern */
  async function brokenBattery(manifest, pattern) {
    await writeFile(batteryJson, JSON.stringify(manifest));
    await rejectsWith(() => discoverNitroModules(project, 'qnx'), pattern);
  }
  await brokenBattery({ ...battery, name: 'other' }, /contains package "other"/);
  await brokenBattery({ ...battery, version: undefined }, /must declare a version/);
  await brokenBattery(
    {
      ...battery,
      'react-native-qnx': { nitro: { ...base, cmakeListsPath: 'missing/CMakeLists.txt' } },
    },
    /react-native-qnx-battery: missing\/CMakeLists\.txt must be a regular/
  );
  await brokenBattery(
    { ...battery, 'react-native-qnx': { nitro: { ...base, target: 'AcmeGauge' } } },
    /declare the same Nitro target AcmeGauge/
  );
  await brokenBattery(
    { ...battery, 'react-native-qnx': { nitro: { ...base, platforms: ['qnx', 'marathon'] } } },
    /^Error: react-native-qnx-battery: package\.json "react-native-qnx"\.nitro\.platforms contains "marathon"/
  );
  await writeFile(batteryJson, '{not json');
  await rejectsWith(() => discoverNitroModules(project, 'qnx'), /JSON/);
  await writeFile(batteryJson, JSON.stringify(battery));
  // A CMakeLists.txt that is a symlink out of the package is refused.
  const cmakeFile = path.join(modules, 'react-native-qnx-battery/qnx/CMakeLists.txt');
  await rm(cmakeFile);
  await writeFile(path.join(temporary, 'outside.txt'), 'add_library(QnxBattery INTERFACE)\n');
  await symlink(path.join(temporary, 'outside.txt'), cmakeFile);
  await rejectsWith(() => discoverNitroModules(project, 'qnx'), /symlinks/);
  // A dependency that is not installed contributes nothing; names must be valid.
  await writeFile(
    path.join(project, 'package.json'),
    JSON.stringify({ ...appManifest, dependencies: { 'not-installed': '1.0.0' } })
  );
  assert.deepEqual(await discoverNitroModules(project, 'qnx'), []);
  checks++;
  await writeFile(
    path.join(project, 'package.json'),
    JSON.stringify({ ...appManifest, dependencies: { '../escape': '1.0.0' } })
  );
  await rejectsWith(() => discoverNitroModules(project, 'qnx'), /not a valid npm package name/);
  const failed = await inspectConsumer(project);
  assert.equal(failed.checks.find((check) => check.id === 'nitro-modules')?.status, 'failed');
  checks++;
} finally {
  await identityTool.release();
  await rm(temporary, { recursive: true, force: true });
}

async function realTemporary() {
  const { realpath } = await import('node:fs/promises');
  return realpath(temporary);
}

console.log(
  `Nitro package metadata: ${checks} checks passed (discovery, descriptor, plan, CMake host compile; no QNX build or device run).`
);
