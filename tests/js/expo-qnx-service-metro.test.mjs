import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtemp, readFile, rm, symlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { createRequire } from 'node:module';
import path from 'node:path';

const root = process.cwd();
const require = createRequire(import.meta.url);
const expoRequire = createRequire(path.join(root, 'packages/expo-modules-core-qnx/package.json'));
const expoCore = path.dirname(path.dirname(expoRequire.resolve('expo-modules-core')));
const rnRequire = createRequire(require.resolve('react-native/package.json'));
const metroManifest = require.resolve('metro/package.json');
const metadata = JSON.parse(await readFile(metroManifest, 'utf8'));
const bin = path.join(
  path.dirname(metroManifest),
  typeof metadata.bin === 'string' ? metadata.bin : metadata.bin.metro
);
const project = await mkdtemp(path.join(tmpdir(), 'rnqnx-expo-services-'));
try {
  await symlink(path.join(root, 'node_modules'), path.join(project, 'node_modules'), 'dir');
  await writeFile(
    path.join(project, 'package.json'),
    JSON.stringify({ name: 'expo-service-probe' })
  );
  await writeFile(
    path.join(project, 'babel.config.js'),
    "module.exports = { presets: ['babel-preset-expo'] };\n"
  );
  await writeFile(
    path.join(project, 'index.js'),
    `import { requireNativeModule, requireOptionalNativeModule } from 'expo-modules-core';
console.log('RNQNX_EXPO_SERVICE_IMPORT', requireOptionalNativeModule('ExpoCamera'));
try { requireNativeModule('ExpoCamera'); } catch (error) { console.log(error.code); }
`
  );
  for (const platform of ['qnx', 'marathon']) {
    const factory = platform === 'qnx' ? 'getQnxMetroConfig' : 'getMarathonMetroConfig';
    const configPath = path.join(project, `metro.${platform}.config.js`);
    await writeFile(
      configPath,
      `const { ${factory} } = require(${JSON.stringify(path.join(root, `packages/metro-config-${platform}/src/index.js`))});
const withPublicEntry = require(${JSON.stringify(path.join(root, 'packages/metro-config-qnx/public-entry.js'))});
module.exports = withPublicEntry(${factory}(__dirname, {
watchFolders: [${JSON.stringify(root)}],
resolver: { nodeModulesPaths: [${JSON.stringify(path.join(root, 'node_modules'))}],
extraNodeModules: { 'expo-modules-core': ${JSON.stringify(expoCore)}, '@babel/runtime': ${JSON.stringify(path.dirname(rnRequire.resolve('@babel/runtime/package.json')))} } }
}), ${JSON.stringify(platform)});
`
    );
    const out = path.join(project, `${platform}.bundle.js`);
    const result = spawnSync(
      process.execPath,
      [
        bin,
        'build',
        'index.js',
        '--config',
        configPath,
        '--platform',
        platform,
        '--dev',
        'false',
        '--max-workers',
        '2',
        '--out',
        out,
      ],
      { cwd: project, encoding: 'utf8', timeout: 120000, maxBuffer: 4 * 1024 * 1024 }
    );
    assert.equal(result.error, undefined);
    assert.equal(result.status, 0, result.stdout + result.stderr);
    const bundle = await readFile(out, 'utf8');
    assert.ok(bundle.includes('RNQNX_EXPO_SERVICE_IMPORT'));
    assert.equal(bundle.includes('ERR_QNX_EXPO_MODULE_UNAVAILABLE'), platform === 'qnx');
    console.log(
      `Expo public service imports bundle on ${platform}; runtime/device acceptance remains separate.`
    );
  }
} finally {
  await rm(project, { recursive: true, force: true });
}
