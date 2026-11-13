// Expo modules in a QNX consumer app: a dependency that declares
// "react-native-qnx": {"expo": {...}} is found by prepare-native, recorded as
// expoModules with the resolved expo config, linked by RnqnxConsumerApp.cmake
// and registered by the generated header. This test uses the real
// @rn-qnx/expo-constants-qnx package and compiles it on the host with React
// Native's JSI sources. It is not a QNX build and runs no JavaScript engine.
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { copyFile, mkdir, mkdtemp, readFile, rm, symlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import {
  discoverExpoModules,
  discoverNitroModules,
  validateNitroDeclaration,
} from '../../packages/react-native-qnx-cli/src/nitro-modules.mjs';
import { prepareNativeMain } from '../../packages/react-native-qnx-cli/src/prepare-native.mjs';
import { planNativeBuild } from '../../packages/react-native-qnx-cli/src/build-native.mjs';
import { useIdentityToolInEnvironment } from './helpers/identity-tool.mjs';

const repo = process.cwd();
const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-expo-modules-'));
const project = path.join(temporary, 'app');
const identityTool = await useIdentityToolInEnvironment();
let checks = 0;

const declaration = JSON.parse(
  await readFile(path.join(repo, 'packages/expo-constants-qnx/package.json'), 'utf8')
)['react-native-qnx'];
assert.deepEqual(validateNitroDeclaration('pkg', declaration, 'expo'), declaration.expo);
assert.throws(
  () => validateNitroDeclaration('pkg', { expo: { ...declaration.expo, modules: [] } }, 'expo'),
  /"react-native-qnx"\.expo\.modules is not supported/
);
checks += 2;

try {
  await mkdir(path.join(project, 'build/qnx'), { recursive: true });
  await writeFile(
    path.join(project, 'app.json'),
    JSON.stringify({
      expo: {
        name: 'Constants App',
        slug: 'constants-app',
        version: '2.3.0',
        icon: './icon.png',
        extra: { rnqnx: { appId: 'org.example.constants', componentName: 'ConstantsApp' } },
      },
    })
  );
  await copyFile(path.join(repo, 'template/qnx/assets/icon.png'), path.join(project, 'icon.png'));
  await writeFile(path.join(project, 'build/qnx/app.hbc'), 'synthetic bytecode\n');
  await writeFile(
    path.join(project, 'package.json'),
    JSON.stringify({
      name: 'constants-app',
      version: '1.0.0',
      private: true,
      dependencies: { '@rn-qnx/expo-constants-qnx': '0.1.0' },
    })
  );
  await mkdir(path.join(project, 'node_modules/@rn-qnx'), { recursive: true });
  await symlink(
    path.join(repo, 'packages/expo-constants-qnx'),
    path.join(project, 'node_modules/@rn-qnx/expo-constants-qnx')
  );

  // Discovery: an Expo declaration, and no Nitro module.
  const found = await discoverExpoModules(project, 'qnx');
  assert.deepEqual(
    found.map((module) => [module.package, module.target, module.initializer]),
    [['@rn-qnx/expo-constants-qnx', 'expo_constants_qnx', 'rnqnx::expo::constants::registerModule']]
  );
  assert.deepEqual(await discoverNitroModules(project, 'qnx'), []);
  checks += 2;

  await prepareNativeMain(['--project', project, '--bundle', 'build/qnx/app.hbc']);
  const descriptorFile = path.join(project, 'build/qnx/native-app.json');
  const descriptor = JSON.parse(await readFile(descriptorFile, 'utf8'));
  assert.deepEqual(descriptor.expoModules, found);
  assert.equal(descriptor.nitroModules, undefined);
  assert.equal(descriptor.expoConfig.name, 'Constants App');
  assert.equal(descriptor.expoConfig.version, '2.3.0');
  assert.ok(Object.keys(descriptor.expoConfig).every((key) => !key.startsWith('_')));
  checks += 5;

  const savedHost = process.env.QNX_HOST;
  const savedTarget = process.env.QNX_TARGET;
  process.env.QNX_HOST = temporary;
  process.env.QNX_TARGET = temporary;
  try {
    const plan = await planNativeBuild(project, 'qnx');
    assert.ok(plan.configure.includes('-DRNQNX_BUILD_EXPO_MODULES_QNX=ON'));
    assert.ok(!plan.configure.includes('-DRNQNX_BUILD_NITRO_QNX=ON'));
    assert.deepEqual(plan.expoModules, ['@rn-qnx/expo-constants-qnx']);
    checks += 3;
  } finally {
    if (savedHost === undefined) delete process.env.QNX_HOST;
    else process.env.QNX_HOST = savedHost;
    if (savedTarget === undefined) delete process.env.QNX_TARGET;
    else process.env.QNX_TARGET = savedTarget;
  }

  // CMake: the host library and the module are linked and the header embeds
  // the config. The probe registers the module and checks the UUID helpers.
  const reactNative = path.join(repo, 'node_modules/react-native');
  const source = path.join(temporary, 'cmake');
  await mkdir(source, { recursive: true });
  await writeFile(
    path.join(source, 'CMakeLists.txt'),
    `cmake_minimum_required(VERSION 3.20)
project(ConsumerExpo LANGUAGES CXX)
set(RNQNX_REACT_NATIVE_DIR "${reactNative}")
include("${repo}/cmake/RnqnxConsumerApp.cmake")
file(READ "${descriptorFile}" config)
rnqnx_consumer_expo_modules("\${config}" "\${CMAKE_BINARY_DIR}" header targets)
add_executable(expo_probe main.cpp "${reactNative}/ReactCommon/jsi/jsi/jsi.cpp")
target_compile_features(expo_probe PRIVATE cxx_std_20)
target_link_libraries(expo_probe PRIVATE \${targets})
target_compile_definitions(expo_probe PRIVATE RNQNX_CONSUMER_EXPO_HEADER="\${header}")
`
  );
  await writeFile(
    path.join(source, 'main.cpp'),
    `#include RNQNX_CONSUMER_EXPO_HEADER
#include "ExpoQnxModuleRegistry.hpp"
#include "ExpoQnxUuid.hpp"
#include <cstdio>
int main() {
  rnqnx::expo::constants::registerModule();
  for (const auto& name : rnqnx::expo::ExpoQnxModuleRegistry::instance().names())
    std::printf("module %s\\n", name.c_str());
  std::printf("v5 %s\\n", rnqnx::expo::uuidV5("www.example.com",
      "6ba7b810-9dad-11d1-80b4-00c04fd430c8").value_or("none").c_str());
  std::printf("bad %s\\n", rnqnx::expo::uuidV5("x", "not-a-uuid").value_or("none").c_str());
  std::printf("v4 %s\\n", rnqnx::expo::uuidV4().c_str());
  return 0;
}
`
  );
  const configured = spawnSync('cmake', ['-S', source, '-B', path.join(source, 'build')], {
    encoding: 'utf8',
    timeout: 60000,
    maxBuffer: 4 * 1024 * 1024,
  });
  assert.equal(configured.status, 0, configured.stdout + configured.stderr);
  assert.match(
    configured.stdout,
    /Consumer Expo module: @rn-qnx\/expo-constants-qnx@0\.1\.0 -> expo_constants_qnx/
  );
  checks += 2;
  const header = await readFile(path.join(source, 'build/ExpoModules.generated.h'), 'utf8');
  assert.match(header, /namespace rnqnx::expo::constants \{\nvoid registerModule\(\);/);
  assert.match(header, /setAppManifestJson\(R"rnqnx_cfg\(/);
  assert.match(header, /"name" : "Constants App"|"name":"Constants App"/);
  assert.match(header, /::rnqnx::expo::installExpoModulesQnx\(runtime\);/);
  checks += 4;
  const built = spawnSync('cmake', ['--build', path.join(source, 'build')], {
    encoding: 'utf8',
    timeout: 300000,
    maxBuffer: 8 * 1024 * 1024,
  });
  assert.equal(built.status, 0, built.stdout + built.stderr);
  const ran = spawnSync(path.join(source, 'build/expo_probe'), { encoding: 'utf8' });
  assert.equal(ran.status, 0, ran.stderr);
  const lines = ran.stdout.trim().split('\n');
  assert.deepEqual(lines.slice(0, 3), [
    'module ExponentConstants',
    // RFC 9562 test vector (DNS namespace, www.example.com).
    'v5 2ed6657d-e927-568b-95e1-2665a8aea6a2',
    'bad none',
  ]);
  assert.match(
    lines[3],
    /^v4 [0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/
  );
  checks += 4;

  // A descriptor whose expoModules lack the config is refused.
  const broken = { ...descriptor };
  delete broken.expoConfig;
  const brokenFile = path.join(temporary, 'broken.json');
  await writeFile(brokenFile, JSON.stringify(broken));
  const brokenSource = path.join(temporary, 'cmake-broken');
  await mkdir(brokenSource, { recursive: true });
  await writeFile(
    path.join(brokenSource, 'CMakeLists.txt'),
    `cmake_minimum_required(VERSION 3.20)
project(ConsumerExpoBroken LANGUAGES CXX)
include("${repo}/cmake/RnqnxConsumerApp.cmake")
file(READ "${brokenFile}" config)
rnqnx_consumer_expo_modules("\${config}" "\${CMAKE_BINARY_DIR}" header targets)
`
  );
  const refused = spawnSync('cmake', ['-S', brokenSource, '-B', path.join(brokenSource, 'b')], {
    encoding: 'utf8',
    timeout: 60000,
  });
  assert.notEqual(refused.status, 0);
  assert.match(refused.stdout + refused.stderr, /need the expoConfig object/);
  checks += 2;
} finally {
  await identityTool.release();
  await rm(temporary, { recursive: true, force: true });
}

console.log(
  `Expo module registration: ${checks} checks passed (discovery, descriptor, plan, CMake host compile; no QNX build or device run).`
);
