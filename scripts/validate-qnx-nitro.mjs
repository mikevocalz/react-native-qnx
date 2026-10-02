#!/usr/bin/env node

import { readFile, access } from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';

const root = process.cwd();
const require = createRequire(import.meta.url);
const manifestPath = require.resolve('react-native-nitro-modules/package.json');
const manifest = JSON.parse(await readFile(manifestPath, 'utf8'));

if (manifest.version !== '0.37.1') {
  throw new Error(`Expected react-native-nitro-modules 0.37.1, got ${manifest.version}`);
}

const nitroRoot = path.dirname(manifestPath);
const requiredSource = [
  'cpp/core/HybridObject.cpp',
  'cpp/entrypoint/InstallNitro.cpp',
  'cpp/entrypoint/InstallNitro.hpp',
  'cpp/registry/HybridObjectRegistry.cpp',
  'cpp/registry/HybridObjectRegistry.hpp',
  'cpp/threading/Dispatcher.cpp',
  'cpp/threading/ThreadPool.cpp',
  'cpp/platform/NitroLogger.hpp',
  'cpp/platform/ThreadUtils.hpp',
];

for (const relative of requiredSource) {
  await access(path.join(nitroRoot, relative));
}

const install = await readFile(path.join(nitroRoot, 'cpp/entrypoint/InstallNitro.hpp'), 'utf8');
if (!install.includes('void install(jsi::Runtime& runtime);')) {
  throw new Error('Nitro synchronous install(jsi::Runtime&) contract changed.');
}

const installSource = await readFile(
  path.join(nitroRoot, 'cpp/entrypoint/InstallNitro.cpp'),
  'utf8'
);
if (!installSource.includes('KnownGlobalPropertyName::NITRO_MODULES_PROXY')) {
  throw new Error('Nitro global proxy installation contract changed.');
}

const globals = await readFile(path.join(nitroRoot, 'cpp/utils/CommonGlobals.cpp'), 'utf8');
if (!globals.includes('return "NitroModulesProxy";')) {
  throw new Error('NitroModulesProxy global name changed.');
}

const proxy = await readFile(
  path.join(nitroRoot, 'cpp/entrypoint/HybridNitroModulesProxy.cpp'),
  'utf8'
);
if (!proxy.includes('registerHybridMethod("createHybridObject"')) {
  throw new Error('Nitro createHybridObject proxy contract changed.');
}

const registry = await readFile(
  path.join(nitroRoot, 'cpp/registry/HybridObjectRegistry.hpp'),
  'utf8'
);
if (!registry.includes('registerHybridObjectConstructor')) {
  throw new Error('Nitro HybridObjectRegistry constructor contract changed.');
}

const qnxRuntime = await readFile(
  path.join(root, 'packages/qnx-nitro-runtime/cpp/QnxNitroRuntime.cpp'),
  'utf8'
);
if (!qnxRuntime.includes('margelo::nitro::install(runtime);')) {
  throw new Error('QNX Nitro runtime must keep the synchronous upstream installer.');
}
if (qnxRuntime.includes('install(runtime,') || qnxRuntime.includes('CallInvokerDispatcher')) {
  throw new Error('QNX Nitro runtime must not invent an async dispatcher in this milestone.');
}

const integrationProbe = await readFile(
  path.join(root, 'vnext/probes/runtime-qnx/main.cpp'),
  'utf8'
);
if (
  !integrationProbe.includes("NitroModulesProxy.createHybridObject('QnxPlatformInfo')") ||
  !integrationProbe.includes('rnqnx::nitro::installQnxNitroRuntime(*runtime)')
) {
  throw new Error('Combined QNX runtime probe no longer exercises Nitro through JSI.');
}

console.log('validate:qnx-nitro passed for react-native-nitro-modules 0.37.1');
