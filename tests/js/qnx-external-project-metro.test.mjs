import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtemp, readFile, rm, symlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';

const root = process.cwd();
const project = await mkdtemp(path.join(tmpdir(), 'rnqnx-external-metro-'));
try {
  // External source tree with the CI-installed dependency graph provisioned as
  // a link. This is NOT a standalone published-package or clean-machine test.
  await symlink(path.join(root, 'node_modules'), path.join(project, 'node_modules'), 'dir');
  await writeFile(path.join(project, 'package.json'), JSON.stringify({ name: 'external-metro' }));
  await writeFile(path.join(project, 'babel.config.js'), "module.exports = { presets: ['babel-preset-expo'] };\n");
  await writeFile(path.join(project, 'index.js'), `const React = require('react');
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const View = require('react-native/Libraries/Components/View/View').default;
const Text = require('react-native/Libraries/Text/Text').default;
AppRegistry.registerComponent('RNQnxPlayground', () => () =>
  React.createElement(View, null, React.createElement(Text, null, 'External project bundle')));
`);
  for (const platform of ['qnx', 'marathon']) {
    const configFile = platform === 'qnx' ? 'metro.config.js' : 'metro.marathon.config.js';
    const factory = platform === 'qnx' ? 'getQnxMetroConfig' : 'getMarathonMetroConfig';
    const packageFile = path.join(root, `packages/metro-config-${platform}/src/index.js`);
    await writeFile(path.join(project, configFile), `const path = require('node:path');
const { createRequire } = require('node:module');
const { ${factory} } = require(${JSON.stringify(packageFile)});
const rnRequire = createRequire(require.resolve('react-native/package.json'));
module.exports = ${factory}(__dirname, {
  watchFolders: [${JSON.stringify(root)}],
  resolver: {
    extraNodeModules: { '@babel/runtime': path.dirname(rnRequire.resolve('@babel/runtime/package.json')) },
    nodeModulesPaths: [${JSON.stringify(path.join(root, 'node_modules'))}],
  },
});
`);
    const result = spawnSync(process.execPath, [
      path.join(root, 'packages/react-native-qnx-cli/src/bin.mjs'),
      'project', 'bundle', '--project', project, '--platform', platform,
    ], { cwd: project, encoding: 'utf8', timeout: 150000, maxBuffer: 4 * 1024 * 1024 });
    assert.equal(result.error, undefined);
    assert.equal(result.status, 0, result.stdout + result.stderr);
    const bundle = await readFile(path.join(project, `build/${platform}/app.bundle.js`), 'utf8');
    assert.ok(bundle.length > 10000, 'Expected a real production Metro bundle');
    assert.ok(bundle.includes('External project bundle'));
    console.log(`external ${platform} Metro: ${Buffer.byteLength(bundle)} bytes; bundle only, no native run`);
  }
} finally {
  await rm(project, { recursive: true, force: true });
}
