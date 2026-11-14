import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtemp, readFile, rm, symlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import {
  inspectTargetRuntimeFile,
  inspectTargetRuntimeText,
} from '../../packages/react-native-qnx-cli/src/target-runtime-doctor.mjs';

// Synthetic observations; no target ran and no runtime qualification is implied.
const observation = {
  schema: 'rnqnx-qnx-runtime-observation/v1',
  capturedAt: '2026-11-14T12:00:00Z',
  target: { board: 'fixture board', image: 'fixture image', sdp: '8.0.5 fixture' },
  services: { screen: true, slogger2: true },
  tmp: true,
  random: true,
  ipv6: true,
  fontconfig: false,
  directories: ['/runtime/lib', '/runtime/plugins', '/runtime/qml', '/app/assets/fonts'],
  files: ['/app/assets/fonts/Test.ttf'],
  environment: {
    LD_LIBRARY_PATH: '',
    QT_PLUGIN_PATH: '',
    QML2_IMPORT_PATH: '',
    QT_QPA_FONTDIR: '',
    QT_QPA_QNX_DISPLAY_CONFIG: '',
    QQNX_PHYSICAL_SCREEN_SIZE: '',
  },
  resolvedDirectories: {
    libraries: '/runtime/lib',
    plugins: '/runtime/plugins',
    qml: '/runtime/qml',
    fonts: ['/app/assets/fonts'],
  },
  display: { widthMm: 530, heightMm: 300, configurationVerified: true },
};
let assertions = 0;
/** @param {unknown} input @param {string} id @param {'passed'|'failed'} status */
function expectCheck(input, id, status) {
  const result = inspectTargetRuntimeText(JSON.stringify(input));
  assert.equal(result.ok, true);
  if (!result.ok) return;
  assert.equal(result.report.checks.find((item) => item.id === id)?.status, status, id);
  if (status === 'failed') assert.equal(result.report.reportedPrerequisitesSatisfied, false);
  assert.equal(result.report.runtimeVerified, false);
  assertions += 1;
}
/** @param {(value: typeof observation) => void} mutate */
function changed(mutate) {
  const value = structuredClone(observation);
  mutate(value);
  return value;
}
expectCheck(observation, 'physical-screen-size', 'passed');
let result = inspectTargetRuntimeText(JSON.stringify(observation), { verbose: true });
assert.equal(result.ok, true);
if (result.ok) {
  assert.equal(result.report.reportedPrerequisitesSatisfied, true);
  assert.equal(result.report.evidenceTier, 'offline-observation');
  assert.equal(result.report.collectionVerified, false);
  assert.equal(result.report.observationOrigin, 'user-supplied');
  assert.deepEqual(result.report.debugEnvironment, {
    QT_DEBUG_PLUGINS: '1',
    LD_DEBUG: '1',
    QT_LOGGING_TO_CONSOLE: '1',
  });
}
for (const service of ['screen', 'slogger2']) {
  expectCheck(
    changed((v) => {
      v.services[/** @type {'screen'|'slogger2'} */ (service)] = false;
    }),
    service,
    'failed'
  );
}
for (const field of ['tmp', 'random', 'ipv6']) {
  expectCheck(
    changed((v) => {
      v[/** @type {'tmp'|'random'|'ipv6'} */ (field)] = false;
    }),
    field,
    'failed'
  );
}
for (const [directory, id] of [
  ['/runtime/lib', 'LD_LIBRARY_PATH'],
  ['/runtime/plugins', 'QT_PLUGIN_PATH'],
  ['/runtime/qml', 'QML2_IMPORT_PATH'],
]) {
  expectCheck(
    changed((v) => {
      v.directories = v.directories.filter((item) => item !== directory);
    }),
    id,
    'failed'
  );
}
// Defaults and application-registered fonts are valid even when env is unset.
for (const id of [
  'LD_LIBRARY_PATH',
  'QT_PLUGIN_PATH',
  'QML2_IMPORT_PATH',
  'fonts',
  'display-configuration',
]) {
  expectCheck(observation, id, 'passed');
}
expectCheck(
  changed((v) => {
    v.files = [];
  }),
  'fonts',
  'failed'
);
expectCheck(
  changed((v) => {
    v.directories.pop();
  }),
  'fonts',
  'failed'
);
expectCheck(
  changed((v) => {
    v.files = [];
    v.fontconfig = true;
  }),
  'fonts',
  'passed'
);
expectCheck(
  changed((v) => {
    v.resolvedDirectories.fonts = ['/runtime/lib/fonts'];
    v.directories.push('/runtime/lib/fonts');
    v.files = ['/runtime/lib/fonts/Test.otf'];
  }),
  'fonts',
  'passed'
);
expectCheck(
  changed((v) => {
    v.display.configurationVerified = false;
  }),
  'display-configuration',
  'failed'
);
expectCheck(
  changed((v) => {
    v.environment.QT_QPA_QNX_DISPLAY_CONFIG = '/runtime/display.conf';
  }),
  'display-configuration',
  'failed'
);
expectCheck(
  changed((v) => {
    v.environment.QT_QPA_QNX_DISPLAY_CONFIG = '/runtime/display.conf';
    v.files.push('/runtime/display.conf');
  }),
  'display-configuration',
  'passed'
);
for (const [width, height] of [
  [0, 0],
  [530, 0],
  [0, 300],
]) {
  expectCheck(
    changed((v) => {
      v.display.widthMm = width;
      v.display.heightMm = height;
    }),
    'physical-screen-size',
    'failed'
  );
}
expectCheck(
  changed((v) => {
    v.display.widthMm = 0;
    v.display.heightMm = 0;
    v.environment.QQNX_PHYSICAL_SCREEN_SIZE = '530,300';
  }),
  'physical-screen-size',
  'passed'
);
for (const fallback of [
  '0,0',
  '530x300',
  '-1,50',
  'Infinity,300',
  '530,300,20',
  '10001,300',
  '$(touch /tmp/rnqnx-DO-NOT-EXECUTE)',
]) {
  expectCheck(
    changed((v) => {
      v.environment.QQNX_PHYSICAL_SCREEN_SIZE = fallback;
    }),
    'physical-screen-size',
    'failed'
  );
}
for (const input of [
  null,
  [],
  {},
  { ...observation, extra: true },
  { ...observation, services: { screen: 'yes', slogger2: true } },
  { ...observation, capturedAt: '2026-02-31T12:00:00Z' },
  { ...observation, target: { ...observation.target, image: 'bad\u001b[31m' } },
  { ...observation, directories: ['/runtime/lib', '/runtime/lib'] },
  { ...observation, files: ['/runtime/../outside'] },
  {
    ...observation,
    resolvedDirectories: { ...observation.resolvedDirectories, libraries: 'relative' },
  },
  { ...observation, environment: { ...observation.environment, SECRET: 'must not echo' } },
  { ...observation, display: { widthMm: -1, heightMm: 300, configurationVerified: true } },
]) {
  const invalid = inspectTargetRuntimeText(JSON.stringify(input));
  assert.equal(invalid.ok, false);
  if (!invalid.ok) assert.equal(invalid.step, 'validate-report');
  assert.doesNotMatch(JSON.stringify(invalid), /must not echo|\u001b/);
  assertions += 1;
}
for (const source of ['not-json', '{"__proto__":{"polluted":true}}', ' '.repeat(65537)]) {
  assert.equal(inspectTargetRuntimeText(source).ok, false);
  assertions += 1;
}
assert.deepEqual(inspectTargetRuntimeText('{}', { platform: 'marathon' }), {
  ok: false,
  step: 'platform',
  message: 'Target runtime observations are QNX-only.',
});

const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-target-doctor-'));
const file = path.join(temporary, 'observations.json');
const cli = path.resolve('packages/react-native-qnx-cli/src/bin.mjs');
/** @param {string[]} args @param {'project'|'root'} [mode] @param {string} [cwd] */
function run(args, mode = 'project', cwd = temporary) {
  const prefix = mode === 'root' ? ['doctor'] : ['project', 'doctor'];
  const child = spawnSync(process.execPath, [cli, ...prefix, ...args], {
    cwd,
    encoding: 'utf8',
    timeout: 10000,
  });
  assert.equal(child.error, undefined);
  return child;
}
try {
  const text = JSON.stringify(observation);
  await writeFile(file, text);
  assert.equal((await inspectTargetRuntimeFile(file)).ok, true);
  const good = run(['--target-runtime-report', file, '--json', '--verbose']);
  assert.equal(good.status, 0, good.stderr);
  assert.equal(JSON.parse(good.stdout).runtimeVerified, false);
  assert.equal(JSON.parse(good.stdout).debugEnvironment.LD_DEBUG, '1');
  assert.equal(await readFile(file, 'utf8'), text);
  const rootGood = run(['--target-runtime-report', file, '--json', '--verbose'], 'root');
  assert.equal(rootGood.status, 0, rootGood.stderr);
  assert.deepEqual(JSON.parse(rootGood.stdout), JSON.parse(good.stdout));
  const rootEquals = run([`--target-runtime-report=${file}`, '--json'], 'root');
  assert.equal(rootEquals.status, 0, rootEquals.stderr);
  assert.equal(JSON.parse(rootEquals.stdout).reportedPrerequisitesSatisfied, true);
  const rootMissing = run(
    ['--target-runtime-report', path.join(temporary, 'missing'), '--json'],
    'root'
  );
  assert.equal(rootMissing.status, 2);
  assert.equal(JSON.parse(rootMissing.stdout).step, 'read-report');
  const rootMarathon = run(
    [
      '--target-runtime-report',
      path.join(temporary, 'missing'),
      '--platform',
      'marathon',
      '--json',
    ],
    'root'
  );
  assert.equal(rootMarathon.status, 2);
  assert.equal(JSON.parse(rootMarathon.stdout).step, 'platform');
  assert.equal(run(['--target-runtime-report'], 'root').status, 2);
  assert.equal(run(['--target-runtime-report', file, '--help'], 'root').status, 0);
  const ordinaryRoot = run(['--dry-run'], 'root', path.resolve('.'));
  assert.equal(ordinaryRoot.status, 0, ordinaryRoot.stderr);
  assert.match(JSON.parse(ordinaryRoot.stdout).args[0], /scripts\/doctor\.sh$/);
  assert.equal(JSON.parse(ordinaryRoot.stdout).args.length, 1);
  const human = run(['--target-runtime-report', file, '--verbose']);
  assert.equal(human.status, 0, human.stderr);
  assert.match(human.stdout, /offline, not device verification/);
  assert.match(human.stdout, /QT_DEBUG_PLUGINS=1/);
  const failure = changed((v) => {
    v.services.screen = false;
  });
  await writeFile(file, JSON.stringify(failure));
  const bad = run(['--target-runtime-report', file, '--json']);
  assert.equal(bad.status, 1);
  assert.equal(JSON.parse(bad.stdout).reportedPrerequisitesSatisfied, false);
  await writeFile(file, 'bad');
  assert.equal(run(['--target-runtime-report', file, '--json']).status, 2);
  for (const args of [
    ['--target-runtime-report', file, '--platform', 'marathon'],
    ['--target-runtime-report', file, '--project', temporary],
    ['--target-runtime-report', file, '--entry', 'index.js'],
    ['--target-runtime-report', file, '--identity-tool', '/never-run'],
    ['--verbose'],
    ['--target-runtime-report', file, '--target-runtime-report', file],
  ])
    assert.equal(run(args).status, 2);
  await writeFile(file, ' '.repeat(65537));
  assert.equal((await inspectTargetRuntimeFile(file)).ok, false);
  await symlink(file, path.join(temporary, 'linked.json'));
  assert.equal((await inspectTargetRuntimeFile(path.join(temporary, 'linked.json'))).ok, false);
  assert.equal((await inspectTargetRuntimeFile(temporary)).ok, false);
  assert.equal((await inspectTargetRuntimeFile(path.join(temporary, 'missing'))).ok, false);
  console.log(
    `QNX target doctor: ${assertions} observation scenarios plus file/CLI boundaries passed (host fixtures only).`
  );
} finally {
  await rm(temporary, { recursive: true, force: true });
}
