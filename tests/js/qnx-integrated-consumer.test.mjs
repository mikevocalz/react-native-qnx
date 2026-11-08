import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import {
  copyFile,
  mkdir,
  mkdtemp,
  readFile,
  realpath,
  rm,
  symlink,
  writeFile,
} from 'node:fs/promises';
import { createRequire } from 'node:module';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { CONSUMER_CONTRACT } from '../../packages/react-native-qnx-cli/src/consumer-contract.mjs';

const root = process.cwd();
const require = createRequire(path.join(root, 'package.json'));
const { findExpoModulesForPlatform } = require(
  path.join(root, 'packages/expo-qnx-autolinking/src/index.js')
);
const project = await realpath(await mkdtemp(path.join(tmpdir(), 'rnqnx-integrated-consumer-')));
const renderer = process.env.RNQNX_INTEGRATED_RENDERER;
const evidence = process.env.RNQNX_INTEGRATED_EVIDENCE;
if (!!renderer !== !!evidence)
  throw new Error('Renderer and evidence directory must be specified together.');
/** @param {string} executable @param {string[]} args @param {number} [timeout] */
function run(executable, args, timeout = 120000) {
  const result = spawnSync(executable, args, {
    cwd: project,
    encoding: 'utf8',
    timeout,
    killSignal: 'SIGKILL',
    maxBuffer: 4 * 1024 * 1024,
  });
  assert.equal(result.error, undefined);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  return result.stdout + result.stderr;
}
try {
  const dependencies = { ...CONSUMER_CONTRACT.packages, ...CONSUMER_CONTRACT.marathonPackages };
  await writeFile(
    path.join(project, 'package.json'),
    JSON.stringify({
      name: 'integrated-qnx-consumer',
      private: true,
      version: '0.1.0',
      packageManager: CONSUMER_CONTRACT.packageManager,
      dependencies,
      expo: { autolinking: { nativeModulesDir: './modules' } },
    })
  );
  for (const name of Object.keys(dependencies)) {
    const source = name.startsWith('@rn-qnx/')
      ? path.join(root, 'packages', name.split('/')[1])
      : path.dirname(require.resolve(`${name}/package.json`));
    const target = path.join(project, 'node_modules', name);
    await mkdir(path.dirname(target), { recursive: true });
    await symlink(source, target, 'dir');
  }
  const rnRequire = createRequire(require.resolve('react-native/package.json'));
  const babelRuntime = path.dirname(rnRequire.resolve('@babel/runtime/package.json'));
  const entry = `const React = require('react');
import { AppRegistry, View, Text, Platform } from 'react-native';
console.info('RNQNX_EXTERNAL_APP_READY platform=' + Platform.OS);
function App() {
  return React.createElement(View, { style: { flex: 1, backgroundColor: '#10212f', padding: 24 } },
    React.createElement(Text, { style: { color: '#ffffff', fontSize: 28 } }, 'One external app'),
    React.createElement(Text, { style: { color: '#ffffff', fontSize: 18 } }, Platform.OS));
}
AppRegistry.registerComponent('RNQnxPlayground', () => App);
`;
  await writeFile(path.join(project, 'index.js'), entry);
  await writeFile(
    path.join(project, 'babel.config.js'),
    "module.exports = { presets: ['babel-preset-expo'] };\n"
  );
  const fixture = path.join(project, 'modules/shared-fixture');
  await mkdir(fixture, { recursive: true });
  await writeFile(
    path.join(fixture, 'package.json'),
    JSON.stringify({ name: '@rnqnx-test/shared-fixture', version: '1.0.0' })
  );
  await writeFile(
    path.join(fixture, 'expo-module.config.json'),
    JSON.stringify({
      platforms: ['qnx', 'marathon'],
      qnx: { target: 'rnqnx_integration_fixture', modules: ['IntegrationFixture'] },
      marathon: { target: 'rnqnx_integration_fixture', modules: ['IntegrationFixture'] },
    })
  );
  await writeFile(
    path.join(fixture, 'CMakeLists.txt'),
    'add_library(rnqnx_integration_fixture INTERFACE)\n'
  );
  const cli = path.join(root, 'packages/react-native-qnx-cli/src/bin.mjs');
  const reports = [];
  if (evidence) await mkdir(evidence, { recursive: true });
  for (const platform of ['qnx', 'marathon']) {
    const factory = platform === 'qnx' ? 'getQnxMetroConfig' : 'getMarathonMetroConfig';
    const config = `const { ${factory} } = require('@rn-qnx/metro-config-${platform}');
const withPublicEntry = require('@rn-qnx/metro-config-qnx/public-entry');
module.exports = withPublicEntry(${factory}(__dirname, {
  watchFolders: [${JSON.stringify(root)}],
  resolver: { extraNodeModules: { '@babel/runtime': ${JSON.stringify(babelRuntime)} } }
}), '${platform}');\n`;
    await writeFile(
      path.join(project, platform === 'qnx' ? 'metro.config.js' : 'metro.marathon.config.js'),
      config
    );
    const doctor = JSON.parse(
      run(process.execPath, [
        cli,
        'project',
        'doctor',
        '--project',
        project,
        '--platform',
        platform,
        '--json',
      ])
    );
    assert.equal(doctor.metadataCompatible, true);
    assert.equal(doctor.runtimeVerified, false);
    const discovery = await findExpoModulesForPlatform(project, platform);
    assert.equal(discovery.length, 1);
    assert.equal(discovery[0].packageName, '@rnqnx-test/shared-fixture');
    const bundleLog = run(process.execPath, [
      cli,
      'project',
      'bundle',
      '--project',
      project,
      '--platform',
      platform,
    ]);
    const bundle = path.join(project, 'build', platform, 'app.bundle.js');
    const bytes = await readFile(bundle);
    assert.ok(bytes.length > 10000);
    assert.equal(await readFile(path.join(project, 'index.js'), 'utf8'), entry);
    const report = {
      platform,
      bundleBytes: bytes.length,
      bundleSha256: createHash('sha256').update(bytes).digest('hex'),
      sourceSha256: createHash('sha256').update(entry).digest('hex'),
      expoModuleCount: discovery.length,
      nativeRegistrationVerified: false,
      linuxHostRendered: false,
      targetDeviceVerified: false,
    };
    if (renderer && evidence) {
      const png = path.join(evidence, `external-${platform}.png`);
      const log = run(renderer, ['--bundle', bundle, '--screenshot', png], 30000);
      assert.ok(log.includes(`RNQNX_EXTERNAL_APP_READY platform=${platform}`));
      assert.ok(log.includes('RNQNX_PLAYGROUND_SCREENSHOT'));
      const image = await readFile(png);
      assert.ok(
        image.length > 8 &&
          image.subarray(0, 8).equals(Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]))
      );
      await writeFile(path.join(evidence, `external-${platform}.log`), log);
      await writeFile(path.join(evidence, `external-${platform}-bundle.log`), bundleLog);
      await writeFile(
        path.join(evidence, `external-${platform}-doctor.json`),
        JSON.stringify(doctor, null, 2)
      );
      await copyFile(bundle, path.join(evidence, `external-${platform}.bundle.js`));
      report.linuxHostRendered = true;
    }
    reports.push(report);
    console.log(
      `integrated external app ${platform}: doctor/discovery/public Metro passed; host render=${report.linuxHostRendered}`
    );
  }
  assert.equal(reports[0].sourceSha256, reports[1].sourceSha256);
  if (evidence) {
    await writeFile(path.join(evidence, 'external-source.js'), entry);
    await writeFile(
      path.join(evidence, 'external-results.json'),
      JSON.stringify(reports, null, 2) + '\n'
    );
  }
} finally {
  await rm(project, { recursive: true, force: true });
}
