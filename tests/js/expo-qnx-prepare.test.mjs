import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { mkdir, mkdtemp, readFile, realpath, rm, stat, symlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { prepareProject, validateDescriptors } from '../../packages/expo-qnx/src/cli.mjs';

const temporary = await realpath(await mkdtemp(path.join(tmpdir(), 'rnqnx-expo-prepare-')));
let checks = 0;
/** @param {unknown} actual @param {unknown} expected */
function equal(actual, expected) { assert.deepEqual(actual, expected); checks += 1; }
/** @param {() => Promise<unknown>} operation @param {RegExp} pattern */
async function rejects(operation, pattern) { await assert.rejects(operation, pattern); checks += 1; }
try {
  const moduleRoot = path.join(temporary, 'local module');
  await mkdir(moduleRoot);
  await writeFile(path.join(moduleRoot, 'CMakeLists.txt'), 'add_library(qnx_fixture INTERFACE)\n');
  const module = {
    packageName: '@rnqnx-test/counter', packageRoot: moduleRoot, version: '1.0.0',
    cmakeListsPath: 'CMakeLists.txt', target: 'qnx_fixture', modules: ['CounterModule'],
  };
  const dry = await prepareProject(temporary, 'build/generated', [module], { dryRun: true });
  equal(dry.action, 'dry-run');
  await rejects(() => stat(path.join(temporary, 'build')), /ENOENT/);
  const created = await prepareProject(temporary, 'build/generated', [module]);
  equal(created.action, 'created');
  equal(created.moduleCount, 1);
  equal(created.nativeBuildVerified, false);
  equal(created.runtimeVerified, false);
  const output = path.join(temporary, 'build/generated');
  const before = await stat(path.join(output, 'manifest.json'));
  equal((await prepareProject(temporary, 'build/generated', [module])).action, 'unchanged');
  equal((await prepareProject(temporary, 'build/generated', [module], { check: true })).action,
    'unchanged');
  equal((await stat(path.join(output, 'manifest.json'))).mtimeMs, before.mtimeMs);
  const config = JSON.parse(await readFile(path.join(output, 'config.json'), 'utf8'));
  equal(config.extra.rnQnx.targetArch, 'aarch64');
  equal(config.extra.rnQnx.autolinkingJson, 'build/generated/modules.json');
  for (const file of created.files) {
    const contents = await readFile(path.join(output, file.name));
    equal(contents.length, file.bytes);
    equal(createHash('sha256').update(contents).digest('hex'), file.sha256);
  }
  const cmake = await readFile(path.join(output, 'modules.cmake'), 'utf8');
  assert.match(cmake, /RNQNX_EXPO_QNX_MODULE_TARGETS qnx_fixture/); checks += 1;
  if (process.platform !== 'win32') {
    equal((await stat(path.join(output, 'modules.json'))).mode & 0o777, 0o600);
  }
  await rejects(() => prepareProject(temporary, 'build/generated', [module], { arch: 'x86_64' }),
    /differs/);
  await writeFile(path.join(output, 'user-note.txt'), 'must survive');
  await rejects(() => prepareProject(temporary, 'build/generated', [module]), /extra files/);
  equal(await readFile(path.join(output, 'user-note.txt'), 'utf8'), 'must survive');
  await rm(path.join(output, 'user-note.txt'));
  await writeFile(path.join(output, 'modules.cmake'), 'manual edit');
  await rejects(() => prepareProject(temporary, 'build/generated', [module]), /differs/);
  equal(await readFile(path.join(output, 'modules.cmake'), 'utf8'), 'manual edit');
  await rejects(() => prepareProject(temporary, 'build/missing', [], { check: true }), /missing/);
  await rejects(() => prepareProject(temporary, '../escape', []), /below/);
  await rejects(() => prepareProject(temporary, 'build', []), /below/);
  await rejects(() => prepareProject(temporary, 'build/new', [], { arch: 'arm32' }), /--arch/);
  await rejects(() => prepareProject(temporary, 'build/new', [], { check: true, dryRun: true }),
    /cannot be combined/);
  await symlink(moduleRoot, path.join(temporary, 'build/link'), 'dir');
  await rejects(() => prepareProject(temporary, 'build/link/new', []), /symlinks/);
  const empty = await prepareProject(temporary, 'build/empty', [], { arch: 'x86_64' });
  equal(empty.moduleCount, 0); equal(empty.targetArch, 'x86_64');
  await rm(path.join(temporary, 'build/empty/config.json'));
  await symlink(path.join(output, 'config.json'), path.join(temporary, 'build/empty/config.json'));
  await rejects(() => prepareProject(temporary, 'build/empty', [], { arch: 'x86_64' }), /unsafe/);

  for (const invalid of [
    { target: 'evil);message(FATAL_ERROR hacked)' }, { target: 'x;y' },
    { packageName: 'invalid name' }, { packageName: 'bad"package' },
    { packageRoot: 'relative' }, { cmakeListsPath: '../CMakeLists.txt' },
    { cmakeListsPath: '/tmp/CMakeLists.txt' }, { cmakeListsPath: 'C:\\bad\\CMakeLists.txt' },
    { cmakeListsPath: 'missing.txt' }, { modules: ['bad;class'] }, { modules: 'Counter' },
  ]) {
    await rejects(() => validateDescriptors([{ ...module, ...invalid }]), /Invalid|absolute|inside|ENOENT|array|CMakeLists/);
  }
  await rejects(() => validateDescriptors(null), /128/);
  await rejects(() => validateDescriptors(Array(129).fill(module)), /128/);
  await rejects(() => validateDescriptors([module, module]), /Duplicate/);
  await rejects(() => validateDescriptors([module, { ...module, packageName: 'other' }]), /Duplicate/);
  await mkdir(path.join(moduleRoot, 'linked'));
  await symlink(path.join(moduleRoot, 'CMakeLists.txt'), path.join(moduleRoot, 'linked/CMakeLists.txt'));
  await rejects(() => validateDescriptors([{ ...module, cmakeListsPath: 'linked/CMakeLists.txt' }]), /regular/);
  const unsafeRoot = path.join(temporary, 'module${UNSAFE}');
  await mkdir(unsafeRoot);
  await writeFile(path.join(unsafeRoot, 'CMakeLists.txt'), 'add_library(unsafe INTERFACE)');
  await rejects(() => validateDescriptors([{ ...module, packageRoot: unsafeRoot }]), /metacharacters/);

  const cli = path.resolve('packages/expo-qnx/src/cli.mjs');
  const help = spawnSync(process.execPath, [cli, '--help'], { encoding: 'utf8', timeout: 10000 });
  equal(help.status, 0);
  for (const args of [
    ['prebuild'], ['prepare', 'extra'], ['inspect', '--out', 'x'],
    ['prepare', '--arch', 'arm32'], ['prepare', '--project', ''],
    ['prepare', '--check', '--dry-run'], ['prepare', '--out', 'x', '--out', 'y'],
  ]) {
    const result = spawnSync(process.execPath, [cli, ...args], { encoding: 'utf8', timeout: 10000 });
    equal(result.status, 1);
  }
  console.log(`expo-qnx-prepare: ${checks} checks passed (host files/generation only).`);
} finally {
  await rm(temporary, { recursive: true, force: true });
}
