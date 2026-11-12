import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import {
  copyFile,
  lstat,
  mkdir,
  mkdtemp,
  readFile,
  rm,
  symlink,
  writeFile,
} from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import {
  nativeConfiguration,
  prepareNativeMain,
  validateAppIdentity,
} from '../../packages/react-native-qnx-cli/src/prepare-native.mjs';
import { identityToolForTests } from './helpers/identity-tool.mjs';

const root = process.cwd();
const identityTool = await identityToolForTests();
process.env.RNQNX_IDENTITY_TOOL = identityTool.tool;
const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-native-config-'));
const project = path.join(temporary, 'one external app');
let checks = 0;
/** @param {boolean} value */
function check(value) {
  assert.ok(value);
  checks++;
}
/** @param {() => Promise<unknown>} operation */
async function rejects(operation) {
  await assert.rejects(operation);
  checks++;
}
const app = {
  expo: {
    name: 'Field Notes — 学習',
    icon: './assets/icon.png',
    extra: { rnqnx: { appId: 'org.rnqnx.notes', componentName: 'FieldNotes', shortName: 'Notes' } },
  },
};
try {
  await mkdir(path.join(project, 'assets'), { recursive: true });
  await copyFile(
    path.join(root, 'template/qnx/assets/icon.png'),
    path.join(project, 'assets/icon.png')
  );
  await writeFile(path.join(project, 'app.json'), JSON.stringify(app));
  for (const platform of ['qnx', 'marathon']) {
    await mkdir(path.join(project, 'build', platform), { recursive: true });
    await writeFile(
      path.join(project, 'build', platform, 'app.bundle.js'),
      'trusted synthetic bundle\n'
    );
    const { configuration } = await nativeConfiguration(project, platform);
    check(configuration.platform === platform && configuration.componentName === 'FieldNotes');
    check(
      configuration.bundleSha256 ===
        createHash('sha256').update('trusted synthetic bundle\n').digest('hex')
    );
    const args = ['--project', project, '--platform', platform];
    await prepareNativeMain([...args, '--dry-run']);
    await rejects(() => lstat(path.join(project, 'build', platform, 'native-app.json')));
    await prepareNativeMain(args);
    const file = path.join(project, 'build', platform, 'native-app.json');
    const before = (await lstat(file)).mtimeMs;
    await prepareNativeMain(args);
    check((await lstat(file)).mtimeMs === before);
    check(JSON.parse(await readFile(file, 'utf8')).appId === 'org.rnqnx.notes');
    await writeFile(path.join(project, 'build', platform, 'app.bundle.js'), 'changed');
    await rejects(() => prepareNativeMain(args));
    await writeFile(
      path.join(project, 'build', platform, 'app.bundle.js'),
      'trusted synthetic bundle\n'
    );
  }
  for (const appId of [
    '',
    'app',
    'org..app',
    '../org.app',
    'org.app/evil',
    'org.app.desktop',
    'org.app\n',
    'org.1app',
    'a'.repeat(256) + '.app',
  ]) {
    assert.throws(() =>
      validateAppIdentity({ appId, displayName: 'Notes', componentName: 'Notes' })
    );
    checks++;
  }
  for (const componentName of ['', '1Notes', 'Bad Name', 'Notes\n', 'a'.repeat(129)]) {
    assert.throws(() =>
      validateAppIdentity({ appId: 'org.example.app', displayName: 'Notes', componentName })
    );
    checks++;
  }
  for (const displayName of [
    '',
    ' Notes',
    'Notes ',
    'Notes\nlog',
    'Notes\u0085log',
    'Notes\u2028log',
    '\ud800',
    'a'.repeat(129),
  ]) {
    assert.throws(() =>
      validateAppIdentity({ appId: 'org.example.app', displayName, componentName: 'Notes' })
    );
    checks++;
  }
  for (const args of [
    ['--project', project, '--platform', 'android'],
    ['--project', project, '--unknown'],
    ['--project', project, '--platform', 'qnx', '--platform', 'marathon'],
    ['--project', ''],
    ['--project', project, 'extra'],
    ['--project', project, '--bundle', '../bundle.js'],
    ['--project', project, '--bundle', 'build/marathon/app.bundle.js'],
  ])
    await rejects(() => prepareNativeMain(args));
  await writeFile(path.join(project, 'app.json'), '{bad json');
  await rejects(() => nativeConfiguration(project));
  await writeFile(path.join(project, 'app.json'), ' '.repeat(65537));
  await rejects(() => nativeConfiguration(project));
  await rm(path.join(project, 'app.json'));
  await writeFile(path.join(temporary, 'app.json'), JSON.stringify(app));
  await symlink(path.join(temporary, 'app.json'), path.join(project, 'app.json'));
  await rejects(() => nativeConfiguration(project));
  await rm(path.join(project, 'app.json'));
  await writeFile(path.join(project, 'app.json'), JSON.stringify(app));
  const bundle = path.join(project, 'build/qnx/app.bundle.js');
  await writeFile(bundle, '');
  await rejects(() => nativeConfiguration(project));
  await rm(bundle);
  await symlink(path.join(temporary, 'app.json'), bundle);
  await rejects(() => nativeConfiguration(project));
  await rm(bundle);
  await writeFile(bundle, 'trusted synthetic bundle\n');

  // Configure the actual target generator with an INTERFACE prerequisite. This
  // tests CMake validation/generation only; the separate native job compiles it.
  const source = path.join(temporary, 'cmake');
  await mkdir(source);
  await writeFile(
    path.join(source, 'CMakeLists.txt'),
    `cmake_minimum_required(VERSION 3.20)
project(ConsumerConfiguration LANGUAGES CXX)
add_library(rnqnx_react_host INTERFACE)
function(rnqnx_enable_strict_warnings target)
endfunction()
include("${root}/cmake/RnqnxConsumerApp.cmake")
rnqnx_add_consumer_app()
`
  );
  const config = path.join(project, 'build/qnx/native-app.json');
  /** @param {string} label @param {string[]} args @param {boolean} good */
  function configure(label, args, good) {
    const result = spawnSync(
      'cmake',
      [
        '-S',
        source,
        '-B',
        path.join(temporary, label),
        `-DRNQNX_CONSUMER_CONFIG=${config}`,
        ...args,
      ],
      { encoding: 'utf8', timeout: 30000, maxBuffer: 1024 * 1024 }
    );
    assert.equal(result.error, undefined);
    assert.equal(result.status === 0, good, result.stdout + result.stderr);
    checks++;
    return result;
  }
  configure('preview', ['-DRNQNX_CONSUMER_HOST_PREVIEW=ON'], true);
  configure('no-false-qnx', [], false);
  const generated = await readFile(
    path.join(temporary, 'preview/consumer/qnx/org.rnqnx.notes/ApplicationIdentity.generated.h'),
    'utf8'
  );
  check(generated.includes('FieldNotes') && generated.includes('学習'));
  check(
    generated.includes(
      'configuredIconSizes() { return {16, 24, 32, 48, 64, 96, 128, 192, 256, 512}; }'
    )
  );
  const tamperedIcon = JSON.parse(await readFile(config, 'utf8'));
  tamperedIcon.icon.sizes[0].sha256 = '0'.repeat(64);
  await writeFile(config, JSON.stringify(tamperedIcon));
  configure('stale-icon', ['-DRNQNX_CONSUMER_HOST_PREVIEW=ON'], false);
  // --skip-identity-tool output: an empty set configures with a warning.
  tamperedIcon.icon.sizes = [];
  await writeFile(config, JSON.stringify(tamperedIcon));
  const skipped = configure('skipped-icon', ['-DRNQNX_CONSUMER_HOST_PREVIEW=ON'], true);
  check(skipped.stderr.includes('Consumer icon set was not generated'));
  check(
    (
      await readFile(
        path.join(
          temporary,
          'skipped-icon/consumer/qnx/org.rnqnx.notes/ApplicationIdentity.generated.h'
        ),
        'utf8'
      )
    ).includes('configuredIconSizes() { return {}; }')
  );
  delete tamperedIcon.icon;
  await writeFile(config, JSON.stringify(tamperedIcon));
  configure('missing-icon', ['-DRNQNX_CONSUMER_HOST_PREVIEW=ON'], false);
  const originalConfig = await readFile(config, 'utf8');
  const tampered = JSON.parse(originalConfig);
  tampered.bundleSha256 = '0'.repeat(64);
  await writeFile(config, JSON.stringify(tampered));
  configure('stale-bundle', ['-DRNQNX_CONSUMER_HOST_PREVIEW=ON'], false);
  await writeFile(config, originalConfig);
  const staged = path.join(temporary, 'staged.bundle.js');
  const stage = spawnSync(
    'cmake',
    [
      `-DSOURCE=${bundle}`,
      `-DDESTINATION=${staged}`,
      `-DEXPECTED_SHA256=${JSON.parse(originalConfig).bundleSha256}`,
      '-P',
      path.join(root, 'cmake/StageConsumerBundle.cmake'),
    ],
    { encoding: 'utf8', timeout: 10000 }
  );
  check(stage.status === 0);
  check((await readFile(staged, 'utf8')) === 'trusted synthetic bundle\n');
  console.log(
    `Consumer native configuration: ${checks} checks passed (real file/hash/CMake generation; native compilation separate).`
  );
} finally {
  await rm(temporary, { recursive: true, force: true });
  if (!identityTool.real)
    await rm(path.dirname(identityTool.tool), { recursive: true, force: true });
}
