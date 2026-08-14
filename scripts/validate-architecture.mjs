#!/usr/bin/env node

import { stat } from 'node:fs/promises';
import path from 'node:path';

const expected = [
  'apps/component-conformance',
  'apps/expo-router-example',
  'apps/native-service-demo',
  'apps/performance-lab',
  'apps/rn-playground',
  'native/common',
  'native/platform/linux-host',
  'native/platform/macos-host',
  'native/platform/qnx/application',
  'native/platform/qnx/screen',
  'native/testing',
  'packages/expo-modules-core-qnx',
  'packages/expo-qnx',
  'packages/expo-qnx-autolinking',
  'packages/expo-qnx-config-plugins',
  'packages/metro-config-qnx',
  'packages/qnx-devtools',
  'packages/qnx-nitro-runtime',
  'packages/react-native-qnx',
  'packages/react-native-qnx-cli',
  'packages/react-native-qnx-codegen',
  'template/qnx',
  'cmake/dependencies',
  'cmake/modules',
  'vnext/cmake',
  'vnext/probes/cxx20',
  'scripts',
  'cmake',
  'docs',
  'tests',
];

const expectedFiles = [
  'CMakeLists.txt',
  'CMakePresets.json',
  'CHANGELOG.md',
  'CONTRIBUTING.md',
  'NOTES.md',
  'TODO.md',
  'package.json',
  'pnpm-lock.yaml',
  'pnpm-workspace.yaml',
  'native/platform/qnx/application/main.cpp',
  'native/platform/qnx/screen/QnxScreenSurface.cpp',
  'native/platform/qnx/screen/QnxScreenSurface.h',
  'docs/handoff/QNX_VALIDATION_QUEUE.md',
  'docs/cited-seams.md',
  'docs/packaging.md',
  'docs/toolchain.md',
  'scripts/validate-g0-a.sh',
  'scripts/run-cpp-host-tests.mjs',
  'scripts/validate-dependencies.mjs',
  'tests/cpp/QnxScreenSurfaceHostTest.cpp',
  'vnext/cmake/qnx-aarch64le.toolchain.cmake',
  'vnext/probes/cxx20/main.cpp',
  'template/qnx/.gitkeep',
  'cmake/dependencies/.gitkeep',
  'cmake/modules/.gitkeep',
];

const trackedScaffolds = [
  'apps/component-conformance',
  'apps/expo-router-example',
  'apps/native-service-demo',
  'apps/performance-lab',
  'apps/rn-playground',
  'native/common',
  'native/platform/linux-host',
  'native/platform/macos-host',
  'native/testing',
  'packages/expo-modules-core-qnx',
  'packages/expo-qnx',
  'packages/expo-qnx-autolinking',
  'packages/expo-qnx-config-plugins',
  'packages/metro-config-qnx',
  'packages/qnx-devtools',
  'packages/qnx-nitro-runtime',
  'packages/react-native-qnx',
  'packages/react-native-qnx-cli',
  'packages/react-native-qnx-codegen',
  'template/qnx',
  'cmake/dependencies',
  'cmake/modules',
];

async function main() {
  const root = process.cwd();
  for (const rel of expected) {
    const full = path.join(root, rel);
    const s = await stat(full);
    if (!s.isDirectory()) {
      throw new Error(`${rel} is not a directory`);
    }
  }
  for (const rel of expectedFiles) {
    const full = path.join(root, rel);
    const s = await stat(full);
    if (!s.isFile()) {
      throw new Error(`${rel} is not a file`);
    }
  }
  for (const rel of trackedScaffolds) {
    const marker = path.join(root, rel, '.gitkeep');
    const s = await stat(marker);
    if (!s.isFile()) {
      throw new Error(`${path.relative(root, marker)} is not a file`);
    }
  }
  console.log('validate:architecture passed.');
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
