import assert from 'node:assert/strict';
import { mkdir, mkdtemp, rm, writeFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import { tmpdir } from 'node:os';
import path from 'node:path';
import vm from 'node:vm';

const root = process.cwd();
const require = createRequire(import.meta.url);
const { runBuild } = require('metro');
const { getDefaultConfig } = require('@react-native/metro-config');
const { getQnxMetroConfig } = require('../../packages/metro-config-qnx/src/index.js');
const { getMarathonMetroConfig } = require('../../packages/metro-config-marathon/src/index.js');
const project = await mkdtemp(path.join(tmpdir(), 'rnqnx-package-exports-'));

try {
  // A package installed outside the workspace, with explicit native exports.
  // This checks Metro's real package resolution, not a mocked resolveRequest.
  const installed = path.join(project, 'node_modules/@rnqnx-fixture/controls');
  await mkdir(installed, { recursive: true });
  await writeFile(path.join(project, 'package.json'), JSON.stringify({ name: 'qt-consumer' }));
  await writeFile(
    path.join(project, 'babel.config.js'),
    `module.exports = { presets: [${JSON.stringify(require.resolve('babel-preset-expo'))}] };\n`
  );
  await writeFile(
    path.join(installed, 'package.json'),
    JSON.stringify({
      name: '@rnqnx-fixture/controls',
      version: '1.0.0',
      exports: {
        '.': {
          qnx: './qnx.js',
          marathon: './marathon.js',
          'react-native': './mobile.native.js',
          default: './default.js',
        },
        './slider': {
          qnx: './qnx-slider.js',
          marathon: './marathon-slider.js',
          'react-native': './slider.native.js',
          default: './default.js',
        },
        './portable': { 'react-native': './portable.js', default: './default.js' },
      },
    })
  );
  for (const target of ['qnx', 'marathon']) {
    await writeFile(path.join(installed, `${target}.js`), `module.exports = '${target}';\n`);
    await writeFile(
      path.join(installed, `${target}-slider.js`),
      `module.exports = '${target}-slider';\n`
    );
  }
  for (const [filename, value] of [
    ['mobile.native.js', 'react-native'],
    ['slider.native.js', 'react-native-slider'],
  ]) {
    await writeFile(
      path.join(installed, filename),
      `globalThis.mobileProviderLoaded = true; module.exports = '${value}';\n`
    );
  }
  await writeFile(path.join(installed, 'portable.js'), "module.exports = 'portable-rn';\n");
  await writeFile(path.join(installed, 'default.js'), "module.exports = 'default';\n");
  const entry = path.join(project, 'index.js');
  await writeFile(
    entry,
    `globalThis.packageResult = {
      root: require('@rnqnx-fixture/controls'),
      slider: require('@rnqnx-fixture/controls/slider'),
      portable: require('@rnqnx-fixture/controls/portable'),
    };\n`
  );

  for (const { target, factory } of [
    { target: 'qnx', factory: getQnxMetroConfig },
    { target: 'marathon', factory: getMarathonMetroConfig },
  ]) {
    const defaults = getDefaultConfig(project);
    const config = factory(project);
    assert.deepEqual(
      config.resolver.unstable_conditionNames,
      defaults.resolver.unstable_conditionNames,
      'React Native global conditions are preserved'
    );
    assert.ok(config.resolver.unstable_conditionsByPlatform[target].includes(target));
    for (const platform of ['ios', 'android', 'web', target === 'qnx' ? 'marathon' : 'qnx']) {
      assert.deepEqual(
        config.resolver.unstable_conditionsByPlatform[platform],
        defaults.resolver.unstable_conditionsByPlatform[platform],
        `${target} must not change ${platform}'s conditions`
      );
    }
    const overrides = {
      resolver: {
        unstable_conditionNames: ['react-native', 'app-condition'],
        unstable_conditionsByPlatform: {
          [target]: ['app-target', target],
          android: ['android-extra'],
          web: ['browser', 'web-extra'],
        },
      },
    };
    const before = structuredClone(overrides);
    const customized = factory(project, overrides);
    assert.deepEqual(overrides, before, 'Project configuration is not mutated');
    assert.deepEqual(customized.resolver.unstable_conditionNames, [
      'react-native',
      'app-condition',
    ]);
    assert.deepEqual(customized.resolver.unstable_conditionsByPlatform[target], [
      'app-target',
      target,
    ]);
    assert.deepEqual(customized.resolver.unstable_conditionsByPlatform.android, ['android-extra']);
    assert.deepEqual(customized.resolver.unstable_conditionsByPlatform.web, [
      'browser',
      'web-extra',
    ]);
    const required = factory(project, {
      resolver: { unstable_conditionsByPlatform: { [target]: ['app-target'] } },
    });
    assert.deepEqual(required.resolver.unstable_conditionsByPlatform[target], [
      'app-target',
      target,
    ]);

    // Run the actual bundles to observe which provider executed. Only fixture
    // modules run in this VM; this is not a React Native/Hermes render test.
    for (const platform of [target, 'ios', 'android', 'web']) {
      /** @type {string[]} */
      const warnings = [];
      const bundleConfig = factory(project, {
        maxWorkers: 2,
        // runBuild accepts an already loaded config; include projectRoot as
        // Metro's loadConfig normally does before invoking it through the CLI.
        watchFolders: [project, root],
        reporter: {
          update(/** @type {any} */ event) {
            if (event.type === 'resolver_warning') warnings.push(event.message);
          },
        },
        serializer: {
          getModulesRunBeforeMainModule: () => [],
          getPolyfills: () => [],
        },
      });
      const bundle = await runBuild(bundleConfig, { entry, platform, dev: false, minify: false });
      const result = JSON.parse(
        vm.runInNewContext(
          `${bundle.code}\nJSON.stringify({ ...globalThis.packageResult, mobile: !!globalThis.mobileProviderLoaded });`,
          {}
        )
      );
      assert.deepEqual(warnings, [], 'All fixture exports resolve without fallback warnings');
      assert.deepEqual(result, {
        root: platform === target ? target : 'react-native',
        slider: platform === target ? `${target}-slider` : 'react-native-slider',
        portable: 'portable-rn',
        mobile: platform !== target,
      });
      console.log(`${target} Metro / ${platform}: installed root, subpath and RN fallback passed`);
    }
  }
} finally {
  await rm(project, { recursive: true, force: true });
}
