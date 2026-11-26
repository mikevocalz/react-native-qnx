import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, mkdirSync, writeFileSync, readFileSync, cpSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { spawnSync } from 'node:child_process';

const repo = resolve(import.meta.dirname, '../..');
const scripts = join(repo, 'scripts/qt-public');
const run = (cmd, args, opts = {}) => spawnSync(cmd, args, { encoding: 'utf8', ...opts });
const temp = () => mkdtempSync(join(tmpdir(), 'rnqnx-pr217-'));

test('cached linker flags preserve old flags while enabling new dtags (idempotently)', (t) => {
  if (run('cmake', ['--version']).status !== 0) return t.skip('cmake unavailable');
  const dir = temp(), src = join(dir, 'src'), build = join(dir, 'build'), qt = join(dir, 'qt');
  mkdirSync(src); mkdirSync(join(qt, 'coin/provisioning/common/shared/cmake_toolchain_files'), { recursive: true });
  writeFileSync(join(qt, 'coin/provisioning/common/shared/cmake_toolchain_files/qnx-toolchain-aarch64le.cmake'), '');
  writeFileSync(join(src, 'CMakeLists.txt'), `cmake_minimum_required(VERSION 3.20)
project(dt_checks NONE)
include("${scripts.replaceAll('\\', '/')}/qt612-public-qnx-aarch64le.cmake")
`);
  const env = { ...process.env, QT_PUBLIC_SRC: qt, QNX_HOST: dir };
  for (let i = 0; i < 2; i++) {
    const args = ['-S', src, '-B', build];
    if (!i) args.push('-DCMAKE_MODULE_LINKER_FLAGS:STRING=-Wl,-rpath,/legacy -Wl,--as-needed');
    const r = run('cmake', args, { env });
    assert.equal(r.status, 0, r.stderr);
  }
  const cache = readFileSync(join(build, 'CMakeCache.txt'), 'utf8');
  const flags = cache.match(/^CMAKE_MODULE_LINKER_FLAGS:STRING=(.*)$/m)?.[1] ?? '';
  assert.match(flags, /-Wl,-rpath,\/legacy/);
  assert.match(flags, /-Wl,--as-needed/);
  assert.equal((flags.match(/--enable-new-dtags/g) ?? []).length, 1);
});

test('bundled application wrapper resolves checkout toolchain from a separate directory', (t) => {
  if (run('cmake', ['--version']).status !== 0) return t.skip('cmake unavailable');
  const dir = temp(), bundle = join(dir, 'bundle/qt-public'), checkout = join(dir, 'checkout'), src = join(dir, 'src');
  mkdirSync(bundle, { recursive: true });
  mkdirSync(join(checkout, 'vnext/cmake'), { recursive: true });
  mkdirSync(src);
  cpSync(join(scripts, 'qnx-app-qt-public.toolchain.cmake'), join(bundle, 'qnx-app-qt-public.toolchain.cmake'));
  writeFileSync(join(checkout, 'vnext/cmake/qnx-aarch64le.toolchain.cmake'), 'set(RNQNX_TEST_CHECKOUT_LOADED TRUE)\n');
  writeFileSync(join(src, 'CMakeLists.txt'), `cmake_minimum_required(VERSION 3.20)
project(bundle_check NONE)
include("${bundle.replaceAll('\\', '/')}/qnx-app-qt-public.toolchain.cmake")
if(NOT RNQNX_TEST_CHECKOUT_LOADED)
  message(FATAL_ERROR "Wrong toolchain")
endif()
`);
  const r = run('cmake', ['-S', src, '-B', join(dir, 'build'), '-DRNQNX_APP_TOOLCHAIN_PATH:FILEPATH=' + join(checkout, 'vnext/cmake/qnx-aarch64le.toolchain.cmake')], { env: { ...process.env, QT_PUBLIC_BUILD: dir } });
  assert.equal(r.status, 0, r.stderr);
});

test('selected stage reruns keep both stage outputs and receipts', () => {
  const dir = temp(), fake = join(dir, 'scripts'), root = join(dir, 'root'), logs = join(dir, 'logs');
  mkdirSync(fake); mkdirSync(root);
  cpSync(join(scripts, 'single-pass-build.sh'), join(fake, 'single-pass-build.sh'));
  writeFileSync(join(fake, 'layout.sh'), 'QT_PUBLIC_ROOT="${QT_PUBLIC_ROOT}"\n');
  writeFileSync(join(fake, 'extract-source.sh'), '#!/bin/bash\necho "attempt $MARKER"\n', { mode: 0o755 });
  const env = { ...process.env, QT_PUBLIC_ROOT: root, STAGE_LOG_DIR: logs, WORK_DIR: root, GUARD_PATH: root, ONLY: 'extract', SAMPLE_S: '3600' };
  for (const marker of ['first', 'second']) {
    const r = run('bash', [join(fake, 'single-pass-build.sh')], { env: { ...env, MARKER: marker } });
    assert.equal(r.status, 0, r.stderr);
  }
  const rows = readFileSync(join(logs, 'stage-attempts.tsv'), 'utf8').trim().split('\n').slice(1).map(s => s.split('\t'));
  assert.equal(rows.length, 2);
  assert.notEqual(rows[0][5], rows[1][5]);
  assert.match(readFileSync(rows[0][5], 'utf8'), /first/);
  assert.match(readFileSync(rows[1][5], 'utf8'), /second/);
  assert.equal(readFileSync(join(logs, 'stages.tsv'), 'utf8').trim().split('\n').length, 3);
});
