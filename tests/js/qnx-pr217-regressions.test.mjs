import test from 'node:test';
import assert from 'node:assert/strict';
import {
  cpSync,
  existsSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  readdirSync,
  rmSync,
  writeFileSync,
} from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { spawnSync } from 'node:child_process';

// Host-only control-flow fixtures. The toolchains use empty/host-cc substitutes;
// these tests do not build Qt/QNX or prove target ELF search paths.
const scripts = resolve(import.meta.dirname, '../../scripts/qt-public');
/** @param {string} command @param {string[]} args @param {Omit<import('node:child_process').SpawnSyncOptionsWithStringEncoding, 'encoding'>} [options] */
const run = (command, args, options = {}) =>
  spawnSync(command, args, { encoding: 'utf8', timeout: 60_000, ...options });
/** @param {import('node:test').TestContext} t */
function temporary(t) {
  const path = mkdtempSync(join(tmpdir(), 'rnqnx-pr217-'));
  t.after(() => rmSync(path, { recursive: true, force: true }));
  return path;
}
/** @param {import('node:test').TestContext} t */
function hasCmake(t) {
  if (run('cmake', ['--version']).status === 0) return true;
  t.skip('Host CMake unavailable; cache/toolchain behavior was not verified');
  return false;
}

test('cached SHARED/MODULE/EXE flags preserve unrelated values and repair last dtags choice idempotently', (t) => {
  if (!hasCmake(t)) return;
  const dir = temporary(t),
    src = join(dir, 'src'),
    qt = join(dir, 'qt');
  mkdirSync(src);
  mkdirSync(join(qt, 'coin/provisioning/common/shared/cmake_toolchain_files'), { recursive: true });
  writeFileSync(
    join(qt, 'coin/provisioning/common/shared/cmake_toolchain_files/qnx-toolchain-aarch64le.cmake'),
    ''
  );
  writeFileSync(
    join(src, 'CMakeLists.txt'),
    `cmake_minimum_required(VERSION 3.20)
project(dt_checks NONE)
if(POLICY CMP0126)
  cmake_policy(SET CMP0126 NEW)
endif()
foreach(kind SHARED MODULE EXE)
  if(DEFINED CACHE{CMAKE_\${kind}_LINKER_FLAGS})
    set(CMAKE_\${kind}_LINKER_FLAGS "$CACHE{CMAKE_\${kind}_LINKER_FLAGS}")
  endif()
endforeach()
include("${scripts.replaceAll('\\', '/')}/qt612-public-qnx-aarch64le.cmake")
file(WRITE "\${CMAKE_BINARY_DIR}/effective-flags.txt" "SHARED=\${CMAKE_SHARED_LINKER_FLAGS}\\nMODULE=\${CMAKE_MODULE_LINKER_FLAGS}\\nEXE=\${CMAKE_EXE_LINKER_FLAGS}\\n")
`
  );
  const env = { ...process.env, QT_PUBLIC_SRC: qt, QNX_HOST: dir };
  const choices = [
    '-Wl,-rpath,/legacy -Wl,--as-needed',
    '-Wl,-rpath,/legacy -Wl,--enable-new-dtags -Wl,--disable-new-dtags -Wl,--as-needed',
    '-Wl,-rpath,/legacy -Wl,--disable-new-dtags -Wl,--enable-new-dtags -Wl,--as-needed',
  ];
  for (const [index, original] of choices.entries()) {
    const build = join(dir, `build-${index}`);
    /** @type {Record<string, string>} */
    const first = {};
    for (let attempt = 0; attempt < 3; attempt++) {
      const args = ['-S', src, '-B', build];
      if (!attempt)
        for (const kind of ['SHARED', 'MODULE', 'EXE'])
          args.push(`-DCMAKE_${kind}_LINKER_FLAGS:STRING=${original}`);
      const result = run('cmake', args, { env });
      assert.equal(result.status, 0, result.stdout + result.stderr);
      const cache = readFileSync(join(build, 'CMakeCache.txt'), 'utf8');
      const effective = readFileSync(join(build, 'effective-flags.txt'), 'utf8');
      for (const kind of ['SHARED', 'MODULE', 'EXE']) {
        const value =
          cache.match(new RegExp(`^CMAKE_${kind}_LINKER_FLAGS:STRING=(.*)$`, 'm'))?.[1] ?? '';
        assert.ok(value.startsWith(original), `${kind} discarded unrelated/original flags`);
        assert.ok(
          value.lastIndexOf('--enable-new-dtags') > value.lastIndexOf('--disable-new-dtags'),
          `${kind} leaves disable-new-dtags effective`
        );
        assert.equal(
          effective.match(new RegExp(`^${kind}=(.*)$`, 'm'))?.[1],
          value,
          `${kind} normal variable shadows the repaired cache`
        );
        if (!attempt) first[kind] = value;
        else assert.equal(value, first[kind], `${kind} appended flags on reconfigure`);
      }
    }
  }
});

test('extracted wrapper carries independent APP_SRC toolchain through an actual host compiler try_compile', (t) => {
  if (!hasCmake(t)) return;
  if (run('cc', ['--version']).status !== 0)
    return t.skip('Host cc unavailable; try_compile was not verified');
  const dir = temporary(t),
    bundle = join(dir, 'bundle/qt-public'),
    checkout = join(dir, 'checkout'),
    src = join(dir, 'src'),
    build = join(dir, 'build');
  mkdirSync(bundle, { recursive: true });
  mkdirSync(join(checkout, 'vnext/cmake'), { recursive: true });
  mkdirSync(src);
  const wrapper = join(bundle, 'qnx-app-qt-public.toolchain.cmake');
  const application = join(checkout, 'vnext/cmake/qnx-aarch64le.toolchain.cmake');
  cpSync(join(scripts, 'qnx-app-qt-public.toolchain.cmake'), wrapper);
  writeFileSync(
    application,
    `set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_C_COMPILER cc)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(RNQNX_TEST_CHECKOUT_LOADED TRUE)
`
  );
  writeFileSync(join(src, 'probe.c'), 'int fixture(void) { return 0; }\n');
  writeFileSync(
    join(src, 'CMakeLists.txt'),
    `cmake_minimum_required(VERSION 3.20)
project(bundle_check C)
if(NOT RNQNX_TEST_CHECKOUT_LOADED)
  message(FATAL_ERROR "Wrong application toolchain")
endif()
try_compile(ok "\${CMAKE_BINARY_DIR}/try" "\${CMAKE_CURRENT_SOURCE_DIR}/probe.c" OUTPUT_VARIABLE detail)
if(NOT ok)
  message(FATAL_ERROR "Actual host try_compile failed: \${detail}")
endif()
`
  );
  const env = { ...process.env, QT_PUBLIC_BUILD: dir };
  const result = run(
    'cmake',
    [
      '--debug-trycompile',
      '-S',
      src,
      '-B',
      build,
      `-DCMAKE_TOOLCHAIN_FILE:FILEPATH=${wrapper}`,
      `-DRNQNX_APP_TOOLCHAIN_PATH:FILEPATH=${application}`,
    ],
    { env }
  );
  assert.equal(result.status, 0, result.stdout + result.stderr);
  const childCache = readFileSync(join(build, 'try/CMakeFiles/CMakeTmp/CMakeCache.txt'), 'utf8');
  assert.ok(
    childCache.includes(`RNQNX_APP_TOOLCHAIN_PATH:UNINITIALIZED=${application}`) ||
      childCache.includes(`RNQNX_APP_TOOLCHAIN_PATH:FILEPATH=${application}`),
    'independent toolchain path did not reach child CMake'
  );
  const missing = run(
    'cmake',
    ['-S', src, '-B', join(dir, 'missing'), `-DCMAKE_TOOLCHAIN_FILE:FILEPATH=${wrapper}`],
    { env }
  );
  assert.notEqual(missing.status, 0);
  assert.match(missing.stderr, /RNQNX_APP_TOOLCHAIN_PATH/);
});

/** @param {import('node:test').TestContext} t */
function stageFixture(t) {
  const dir = temporary(t),
    fake = join(dir, 'scripts'),
    root = join(dir, 'root'),
    logs = join(dir, 'logs'),
    bin = join(dir, 'bin');
  mkdirSync(fake);
  mkdirSync(root);
  mkdirSync(bin);
  for (const name of [
    'single-pass-build.sh',
    'layout.sh',
    'qnx-patch-set.sh',
    'assert-clean-tree.sh',
    'guarded-run.sh',
    'guarded-run.py',
  ])
    cpSync(join(scripts, name), join(fake, name));
  writeFileSync(join(fake, 'extract-source.sh'), '#!/bin/bash\necho "attempt $MARKER"\n', {
    mode: 0o755,
  });
  writeFileSync(join(fake, 'configure-host.sh'), '#!/bin/bash\necho "guarded attempt $MARKER"\n', {
    mode: 0o755,
  });
  // The production orchestration runs in Linux; give the macOS host fixture
  // the same GNU df output shape without claiming an actual disk measurement.
  writeFileSync(join(bin, 'df'), '#!/bin/sh\nprintf "Avail\\n999999999\\n"\n', { mode: 0o755 });
  const env = {
    ...process.env,
    PATH: `${bin}:${process.env.PATH}`,
    QT_PUBLIC_ROOT: root,
    QT_PUBLIC_BUILD: join(dir, 'build'),
    STAGE_LOG_DIR: logs,
    WORK_DIR: root,
    GUARD_PATH: root,
    ONLY: 'extract host',
    SAMPLE_S: '0.05',
    GUARD_INTERVAL_S: '0.01',
    MIN_KB: '0',
    REQUIRE_CLEAN: '0',
    QNX_SOFTWARE_GRAB: '0',
  };
  return { dir, fake, root, logs, bin, env };
}

test('ONLY reruns preserve each stage/detail log and seven-column receipts; strict preflight precedes log mutation', (t) => {
  const { fake, root, logs, env } = stageFixture(t);
  for (const marker of ['first', 'second']) {
    const result = run('bash', [join(fake, 'single-pass-build.sh')], {
      env: { ...env, MARKER: marker },
    });
    assert.equal(result.status, 0, result.stdout + result.stderr);
  }
  const attempts = join(logs, 'stage-attempts.tsv');
  const bytes = readFileSync(attempts, 'utf8');
  const rows = bytes
    .trim()
    .split('\n')
    .slice(1)
    .map((line) => line.split('\t'));
  assert.deepEqual(
    rows.map((row) => row[0]),
    ['extract', 'host-tools', 'extract', 'host-tools']
  );
  assert.equal(new Set(rows.map((row) => row[5])).size, 4);
  assert.match(readFileSync(rows[0][5], 'utf8'), /first/);
  assert.match(readFileSync(rows[2][5], 'utf8'), /second/);
  assert.notEqual(rows[1][6], rows[3][6]);
  assert.match(readFileSync(rows[1][6], 'utf8'), /guarded attempt first/);
  assert.match(readFileSync(rows[3][6], 'utf8'), /guarded attempt second/);
  const stages = readFileSync(join(logs, 'stages.tsv'), 'utf8');
  assert.equal(stages.trim().split('\n').length, 5);
  assert.ok(
    stages
      .trim()
      .split('\n')
      .every((line) => line.split('\t').length === 7)
  );
  const entries = readdirSync(logs).sort();
  writeFileSync(join(root, 'src'), 'reused source');
  const rejected = run('bash', [join(fake, 'single-pass-build.sh')], {
    env: { ...env, REQUIRE_CLEAN: '1', MARKER: 'must-not-start' },
  });
  assert.notEqual(rejected.status, 0);
  assert.equal(readFileSync(attempts, 'utf8'), bytes);
  assert.equal(readFileSync(join(logs, 'stages.tsv'), 'utf8'), stages);
  assert.deepEqual(readdirSync(logs).sort(), entries);
});

test('failed attempt-directory creation stops before stages or timing receipts', (t) => {
  const { fake, logs, bin, env } = stageFixture(t);
  writeFileSync(join(bin, 'mktemp'), '#!/bin/sh\nexit 17\n', { mode: 0o755 });
  const result = run('bash', [join(fake, 'single-pass-build.sh')], {
    env: { ...env, MARKER: 'must-not-start' },
  });
  assert.equal(result.status, 17, result.stdout + result.stderr);
  assert.ok(!existsSync(join(logs, 'stages.tsv')));
  assert.ok(!existsSync(join(logs, 'stage-attempts.tsv')));
  assert.doesNotMatch(result.stdout, /=== .* start/);
});
