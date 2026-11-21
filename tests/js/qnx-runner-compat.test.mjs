import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { gunzipSync } from 'node:zlib';
import {
  analyzeProject,
  stockRunnerVerdict,
} from '../../packages/react-native-qnx-cli/src/analyze.mjs';
import {
  listTarGz,
  validateRunnerManifest,
} from '../../packages/react-native-qnx-cli/src/runner.mjs';
import { packRunner } from '../../packages/react-native-qnx-cli/src/runner-pack.mjs';
import { launchCheck } from '../../packages/react-native-qnx-cli/src/runner-tools.mjs';
import { useIdentityToolInEnvironment } from './helpers/identity-tool.mjs';
import { runProject } from '../../packages/react-native-qnx-cli/src/run.mjs';
import {
  compareRunnerToBundle,
  formatMismatch,
  readHermesBytecodeVersion,
  readRuntimeDescriptor,
  validateRuntimeInfo,
} from '../../packages/react-native-qnx-cli/src/runtime-compat.mjs';

// Host evidence only. The bundles are compiled by the hermesc this checkout
// pins (hermes-compiler from react-native 0.88.1); the runner archives are
// packed from a staged directory of synthetic files. No runner binary runs
// here and nothing in this file touches a board.
const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-compat-'));
await useIdentityToolInEnvironment();
let checks = 0;
const ok = (/** @type {unknown} */ v, /** @type {string} */ m) => {
  assert.ok(v, m);
  checks++;
};
const equal = (/** @type {unknown} */ a, /** @type {unknown} */ b, /** @type {string} */ m) => {
  assert.equal(a, b, m);
  checks++;
};
const rejects = async (/** @type {Promise<unknown>} */ p, /** @type {RegExp} */ re) => {
  await assert.rejects(p, re);
  checks++;
};

const rnRequire = createRequire(
  createRequire(import.meta.url).resolve('react-native/package.json')
);
const hermescDirectory = /** @type {Record<string, string>} */ ({
  darwin: 'osx-bin',
  linux: 'linux64-bin',
})[process.platform];
if (!hermescDirectory) throw new Error(`No pinned hermesc for host ${process.platform}.`);
const hermesc = path.join(
  path.dirname(rnRequire.resolve('hermes-compiler/package.json')),
  'hermesc',
  hermescDirectory,
  'hermesc'
);
// hermes-compiler 260318099.0.5 writes bytecode version 99 (measured by
// reading the header of a hermesc-compiled file, 2026-11-20, macOS host).
const PINNED_BYTECODE_VERSION = 99;
const NITRO = '1.0.0-alpha.nightly.136.gdb81de61bd32';

/** @param {string} file @param {string} source */
const compile = (file, source) => {
  const js = `${file}.js`;
  return writeFile(js, source).then(() => {
    const result = spawnSync(hermesc, ['-emit-binary', '-out', file, js], { encoding: 'utf8' });
    assert.equal(result.status, 0, result.stderr);
  });
};

/** @param {string} root @param {string} name @param {Record<string, unknown>} manifest */
const installPackage = async (root, name, manifest) => {
  const directory = path.join(root, 'node_modules', ...name.split('/'));
  await mkdir(directory, { recursive: true });
  await writeFile(
    path.join(directory, 'package.json'),
    JSON.stringify({ name, version: '1.0.0', ...manifest })
  );
  return directory;
};

/** @param {string} root @param {{nitro?: string | null, extra?: (root: string) => Promise<string[]>, rn?: string}} [options] */
const makeApp = async (root, options = {}) => {
  await mkdir(root, { recursive: true });
  const dependencies = ['react-native'];
  await installPackage(root, 'react-native', { version: options.rn ?? '0.88.1' });
  if (options.nitro !== null) {
    await installPackage(root, 'nitro-core', { version: options.nitro ?? NITRO });
    dependencies.push('nitro-core');
  }
  dependencies.push(...((await options.extra?.(root)) ?? []));
  await writeFile(
    path.join(root, 'package.json'),
    JSON.stringify({
      name: 'fixture-app',
      version: '1.0.0',
      dependencies: Object.fromEntries(dependencies.map((d) => [d, '*'])),
    })
  );
  await writeFile(
    path.join(root, 'app.json'),
    JSON.stringify({
      expo: {
        name: 'Fixture',
        slug: 'fixture',
        extra: { rnqnx: { appId: 'com.example.fixture', componentName: 'Fixture' } },
      },
    })
  );
  return root;
};

/** A package that declares a QNX native module of one kind. */
const nativePackage =
  (/** @type {string} */ name, /** @type {'nitro' | 'expo'} */ kind, version = '1.0.0') =>
  async (/** @type {string} */ root) => {
    const directory = await installPackage(root, name, {
      version,
      'react-native-qnx': {
        [kind]: {
          platforms: ['qnx'],
          cmakeListsPath: 'CMakeLists.txt',
          target: name.replace(/[^a-z0-9]/g, '_'),
          initializer: `fixture::${name.replace(/[^a-z0-9]/g, '_')}::registerModule`,
        },
      },
    });
    await writeFile(path.join(directory, 'CMakeLists.txt'), '# fixture\n');
    return name;
  };

const fabricPackage =
  (/** @type {string} */ name, version = '1.0.0') =>
  async (/** @type {string} */ root) => {
    await installPackage(root, name, {
      version,
      codegenConfig: { name: 'FixtureSpec', type: 'components', jsSrcsDir: 'src' },
    });
    return name;
  };

const stage = path.join(temporary, 'stage');
await mkdir(path.join(stage, 'bin'), { recursive: true });
await mkdir(path.join(stage, 'licenses'), { recursive: true });
await writeFile(path.join(stage, 'bin/rnqnx-dev-runner'), 'synthetic runner binary\n');
await writeFile(path.join(stage, 'licenses/LGPL-3.0-only.txt'), 'synthetic notice\n');
const versions = {
  reactNativeQnx: '0.1.0',
  reactNative: '0.88.1',
  hermes: 'hermes-v260318099.0.5',
  qt: '6.12.0',
  sdp: null,
};
const builtins = {
  nitroModules: [{ package: 'fixture-nitro', version: '1.0.0' }],
  expoModules: [{ package: 'fixture-expo', version: '1.0.0' }],
  fabricComponents: [{ package: 'fixture-fabric', version: '1.0.0' }],
};

try {
  // --- Hermes bytecode header, read from a file hermesc wrote.
  const hbc = path.join(temporary, 'real.hbc');
  await compile(hbc, 'globalThis.answer = 42;');
  equal(
    await readHermesBytecodeVersion(hbc),
    PINNED_BYTECODE_VERSION,
    'hermesc output header carries the pinned bytecode version'
  );
  const source = path.join(temporary, 'source.js');
  await writeFile(source, 'globalThis.answer = 42;');
  equal(
    await readHermesBytecodeVersion(source),
    null,
    'a JavaScript bundle has no bytecode version'
  );
  const tiny = path.join(temporary, 'tiny.hbc');
  await writeFile(tiny, Buffer.from('c61fbc03', 'hex'));
  equal(await readHermesBytecodeVersion(tiny), null, 'a truncated header is not bytecode');

  // --- Pack a runner from a stage; the archive carries rnqnx-runtime.json.
  const out1 = path.join(temporary, 'out1');
  const packed = await packRunner({
    stage,
    out: out1,
    id: 'macos-dev-test',
    target: 'macos-arm64',
    versions,
    hermesc,
    nitroAbi: NITRO,
    builtins,
    publishedAt: '2026-11-20T00:00:00Z',
  });
  equal(
    packed.entry.runtime.hermesBytecodeVersion,
    PINNED_BYTECODE_VERSION,
    'pack measured the version'
  );
  const names = await listTarGz(packed.archive);
  ok(names.includes('rnqnx-runtime.json'), 'archive holds rnqnx-runtime.json');
  equal(names.length, 3, 'archive holds the staged files plus the descriptor');
  const archiveBytes = await readFile(packed.archive);
  equal(
    createHash('sha256').update(archiveBytes).digest('hex'),
    packed.entry.sha256,
    'manifest hash matches the archive'
  );
  equal(archiveBytes.length, packed.entry.size, 'manifest size matches the archive');
  const body = gunzipSync(archiveBytes).toString('latin1');
  ok(body.includes('"hermesBytecodeVersion": 99'), 'descriptor is inside the tar stream');

  const again = await packRunner({
    stage,
    out: path.join(temporary, 'out2'),
    id: 'macos-dev-test',
    target: 'macos-arm64',
    versions,
    hermesc,
    nitroAbi: NITRO,
    builtins,
    publishedAt: '2026-11-20T00:00:00Z',
  });
  equal(again.entry.sha256, packed.entry.sha256, 'the same stage packs to the same hash');
  const manifest = validateRunnerManifest(JSON.parse(await readFile(packed.manifest, 'utf8')));
  equal(manifest.runners[0].runtime?.nitroAbi, NITRO, 'fetch-side validation keeps runtime');
  await rejects(
    packRunner({
      stage,
      out: out1,
      id: 'macos-dev-test',
      target: 'macos-arm64',
      versions,
      hermesc,
      nitroAbi: NITRO,
      builtins,
    }),
    /EEXIST/
  );

  // The runner's own descriptor, as a developer reads it after unpacking.
  const descriptorFile = path.join(temporary, 'rnqnx-runtime.json');
  await writeFile(
    descriptorFile,
    JSON.stringify({
      schema: 'rnqnx-runtime/v1',
      runnerId: packed.entry.id,
      target: packed.entry.target,
      versions,
      ...packed.entry.runtime,
    })
  );
  const runtime = await readRuntimeDescriptor(descriptorFile);

  // The archive is readable by the system tar, including a name over 100 bytes.
  const longStage = path.join(temporary, 'long-stage');
  const longDirectory = path.join(longStage, 'a'.repeat(60), 'b'.repeat(60));
  await mkdir(longDirectory, { recursive: true });
  await writeFile(path.join(longDirectory, 'file.txt'), 'long path body\n');
  const long = await packRunner({
    stage: longStage,
    out: path.join(temporary, 'out-long'),
    id: 'macos-long-test',
    target: 'macos-arm64',
    versions,
    hermesc,
    nitroAbi: NITRO,
    builtins,
  });
  const longName = `${'a'.repeat(60)}/${'b'.repeat(60)}/file.txt`;
  ok((await listTarGz(long.archive)).includes(longName), 'long name lists back');
  const extracted = path.join(temporary, 'long-extract');
  await mkdir(extracted);
  const untar = spawnSync('tar', ['-xzf', long.archive, '-C', extracted], { encoding: 'utf8' });
  equal(untar.status, 0, `system tar extracts the archive: ${untar.stderr}`);
  equal(
    await readFile(path.join(extracted, longName), 'utf8'),
    'long path body\n',
    'system tar restores the long-named file'
  );
  const extractedRuntime = JSON.parse(
    await readFile(path.join(extracted, 'rnqnx-runtime.json'), 'utf8')
  );
  equal(
    extractedRuntime.hermesBytecodeVersion,
    PINNED_BYTECODE_VERSION,
    'system tar restores the descriptor'
  );

  // --- SDP boundary and malformed runtime blocks are refused at pack time.
  const sdpStage = path.join(temporary, 'sdp-stage');
  await mkdir(sdpStage);
  await writeFile(path.join(sdpStage, 'libscreen.so.1'), 'x');
  await rejects(
    packRunner({
      stage: sdpStage,
      out: path.join(temporary, 'out-sdp'),
      id: 'qnx-dev-test',
      target: 'qnx-aarch64-pi400',
      versions: { ...versions, sdp: '8.0.5' },
      hermesc,
      nitroAbi: NITRO,
      builtins,
      targetLibraries: ['libc.so.6'],
    }),
    /QNX SDP or target-image files/
  );
  for (const bad of [
    { hermesBytecodeVersion: 0, nitroAbi: NITRO, builtins },
    { hermesBytecodeVersion: 99, nitroAbi: '', builtins },
    {
      hermesBytecodeVersion: 99,
      nitroAbi: NITRO,
      builtins: { ...builtins, fabricComponents: 'x' },
    },
    { hermesBytecodeVersion: 99, nitroAbi: NITRO, builtins: { ...builtins, extra: [] } },
    { hermesBytecodeVersion: 99, nitroAbi: NITRO, builtins, surprise: 1 },
  ])
    await assert.rejects(async () => validateRuntimeInfo(bad, 'runtime'), /runtime/);
  checks += 5;

  // --- Compatibility: each field, both sides named.
  const profile = {
    reactNative: '0.88.1',
    hermesBytecodeVersion: PINNED_BYTECODE_VERSION,
    nitroAbi: NITRO,
    bundleFile: hbc,
  };
  ok(compareRunnerToBundle(runtime, profile).ok, 'matching runner and bundle pass');
  const rn = compareRunnerToBundle(runtime, { ...profile, reactNative: '0.89.0' });
  ok(!rn.ok && rn.mismatches[0].field === 'reactNative', 'React Native mismatch found');
  const hb = compareRunnerToBundle(runtime, { ...profile, hermesBytecodeVersion: 100 });
  ok(!hb.ok && hb.mismatches[0].field === 'hermesBytecodeVersion', 'bytecode mismatch found');
  const nb = compareRunnerToBundle(runtime, {
    ...profile,
    nitroAbi: '1.0.0-alpha.nightly.105.g69d365fc9b18',
  });
  ok(!nb.ok && nb.mismatches[0].field === 'nitroAbi', 'Nitro ABI mismatch found');
  const all = formatMismatch('macos-dev-test', [
    ...rn.mismatches,
    ...hb.mismatches,
    ...nb.mismatches,
  ]);
  for (const text of [
    'runner 0.88.1',
    'app 0.89.0',
    'runner 99',
    'app 100',
    `runner ${NITRO}`,
    'alpha.nightly.105',
  ])
    ok(all.includes(text), `error names ${text}`);
  const open = compareRunnerToBundle(runtime, {
    reactNative: '0.88.1',
    hermesBytecodeVersion: null,
    nitroAbi: null,
    bundleFile: source,
  });
  ok(open.ok && open.notChecked.length === 2, 'unchecked fields are reported, not passed');

  // --- launchCheck against a real project and a real hbc.
  const good = await makeApp(path.join(temporary, 'good'));
  const checked = await launchCheck({ runtime: descriptorFile, project: good, bundle: hbc });
  ok(checked.ok && checked.notChecked.length === 0, 'matching app launches');

  const oldRn = await makeApp(path.join(temporary, 'old-rn'), { rn: '0.87.0' });
  await rejects(
    launchCheck({ runtime: descriptorFile, project: oldRn, bundle: hbc }),
    /runner 0\.88\.1\s+app 0\.87\.0/
  );
  const oldNitro = await makeApp(path.join(temporary, 'old-nitro'), {
    nitro: '1.0.0-alpha.nightly.105.g69d365fc9b18',
  });
  await rejects(
    launchCheck({ runtime: descriptorFile, project: oldNitro, bundle: hbc }),
    /Nitro ABI\s+runner 1\.0\.0-alpha\.nightly\.136\.gdb81de61bd32\s+app 1\.0\.0-alpha\.nightly\.105/
  );
  const staleHbc = path.join(temporary, 'stale.hbc');
  const header = Buffer.from(await readFile(hbc));
  header.writeUInt32LE(98, 8);
  await writeFile(staleHbc, header);
  await rejects(
    launchCheck({ runtime: descriptorFile, project: good, bundle: staleHbc }),
    /Hermes bytecode\s+runner 99\s+app 98/
  );
  await rejects(
    launchCheck({ runtime: path.join(temporary, 'missing.json'), project: good, bundle: hbc }),
    /Cannot read the runner details/
  );
  const noNitro = await makeApp(path.join(temporary, 'no-nitro'), { nitro: null });
  const skipped = await launchCheck({ runtime: descriptorFile, project: noNitro, bundle: hbc });
  ok(
    skipped.notChecked.some((n) => n.startsWith('Nitro ABI')),
    'no Nitro dependency is reported'
  );

  // Nothing compared is not a pass: no react-native, no bundle, no Nitro.
  const bare = path.join(temporary, 'bare');
  await mkdir(bare);
  await writeFile(
    path.join(bare, 'package.json'),
    JSON.stringify({ name: 'bare', dependencies: {} })
  );
  await rejects(
    launchCheck({ runtime: descriptorFile, project: bare }),
    /Nothing could be compared with runner macos-dev-test/
  );
  equal(checked.compared, 3, 'all three fields compared for a full match');
  equal(skipped.compared, 2, 'two fields compared when the app has no Nitro');

  // --- project run stops on a mismatch before it builds anything.
  await mkdir(path.join(oldRn, 'build/qnx'), { recursive: true });
  await writeFile(path.join(oldRn, 'build/qnx/app.bundle.hbc'), header);
  await rejects(
    runProject({ project: oldRn, runtime: descriptorFile }),
    /This runner cannot load this app[\s\S]*React Native\s+runner 0\.88\.1\s+app 0\.87\.0/
  );
  await mkdir(path.join(good, 'build/qnx'), { recursive: true });
  await writeFile(path.join(good, 'build/qnx/app.bundle.hbc'), await readFile(hbc));
  const run = await runProject({ project: good, runtime: descriptorFile });
  const status = Object.fromEntries(run.steps.map((s) => [s.step, s.status]));
  equal(status['runner-check'], 'done', 'a matching runner passes the check inside project run');
  equal(status['native-build'], 'not run', 'a prebuilt runner skips the native build');
  equal(status.deploy, 'needs device', 'project run still stops before the board');
  equal(status.launch, 'not run', 'nothing was launched');

  // --- analyze
  const none = await makeApp(path.join(temporary, 'plain'));
  const plain = await analyzeProject({ project: none });
  equal(plain.stockRunner.sufficient, true, 'no native modules: stock runner is enough');
  const rich = await makeApp(path.join(temporary, 'rich'), {
    extra: async (root) => [
      await nativePackage('fixture-nitro', 'nitro')(root),
      await nativePackage('fixture-expo', 'expo')(root),
      await fabricPackage('fixture-fabric')(root),
    ],
  });
  const undetermined = await analyzeProject({ project: rich });
  equal(
    undetermined.stockRunner.sufficient,
    null,
    'modules without a runner description: undetermined'
  );
  equal(undetermined.nitroModules.length, 1, 'Nitro module listed');
  equal(undetermined.expoModules.length, 1, 'Expo module listed');
  equal(undetermined.fabricComponents.length, 1, 'Fabric component listed');
  const covered = await analyzeProject({ project: rich, runtime: descriptorFile });
  equal(covered.stockRunner.sufficient, true, 'runner links all three modules');
  const extra = await makeApp(path.join(temporary, 'extra'), {
    extra: async (root) => [
      await nativePackage('fixture-nitro', 'nitro')(root),
      await nativePackage('fixture-added', 'expo')(root),
      await fabricPackage('fixture-fabric', '2.0.0')(root),
    ],
  });
  const missing = await analyzeProject({ project: extra, runtime: descriptorFile });
  equal(missing.stockRunner.sufficient, false, 'an unlinked module needs a custom build');
  equal(missing.stockRunner.uncovered.length, 2, 'two modules are uncovered');
  ok(
    /fixture-added 1\.0\.0 \(not in runner\)/.test(missing.stockRunner.reason),
    'reason names the missing module'
  );
  ok(
    /fixture-fabric 2\.0\.0 \(runner has 1\.0\.0\)/.test(missing.stockRunner.reason),
    'reason names the version change'
  );
  equal(
    stockRunnerVerdict({ nitroModules: [], expoModules: [], fabricComponents: [] }, null)
      .sufficient,
    true,
    'empty app verdict'
  );
  const wrongRn = await analyzeProject({ project: oldRn, runtime: descriptorFile });
  equal(
    wrongRn.stockRunner.sufficient,
    false,
    'no modules but the wrong React Native: not sufficient'
  );
  ok(
    /React Native \(runner 0\.88\.1, app 0\.87\.0\)/.test(wrongRn.stockRunner.reason),
    'reason names both React Native versions'
  );
  await rejects(analyzeProject({ project: none, platform: 'ios' }), /qnx or marathon/);

  console.log(`qnx-runner-compat: ${checks} checks passed`);
} finally {
  await rm(temporary, { recursive: true, force: true });
}
