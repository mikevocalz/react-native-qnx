import assert from 'node:assert/strict';
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const temporary = mkdtempSync(join(tmpdir(), 'rnqnx-constants-'));
let sequence = 0;
/** @param {string} config @param {RegExp} expected */
function configure(config, expected) {
  const directory = join(temporary, String(sequence++));
  mkdirSync(directory);
  const file = join(directory, 'public.json');
  writeFileSync(file, config);
  writeFileSync(
    join(directory, 'CMakeLists.txt'),
    `cmake_minimum_required(VERSION 3.31)
project(ConstantsConfig NONE)
add_library(constants STATIC dummy.cpp)
include("${root}/cmake/RnqnxExpoConstants.cmake")
set(RNQNX_ENABLE_EXPO_CONSTANTS ON)
set(RNQNX_EXPO_PUBLIC_CONFIG "${file}")
rnqnx_configure_expo_constants(constants)
`
  );
  const result = spawnSync('cmake', ['-S', directory, '-B', join(directory, 'build')], {
    encoding: 'utf8',
  });
  assert.ifError(result.error);
  assert.notEqual(result.status, 0);
  assert.match(result.stdout + result.stderr, expected);
}
try {
  configure('{', /must be a JSON object/);
  configure('[]', /must be a JSON object/);
  configure('{"expo":{"name":"private config"}}', /not an app.json expo wrapper/);
  configure('{"scheme":"file:\/\/bad"}', /valid URI scheme/);
  configure('{"scheme":42}', /single string scheme/);
  configure(' '.repeat(1048577), /exceeds 1 MiB/);
  console.log('expo-constants-config.test.mjs passed (6 fail-closed configuration cases)');
} finally {
  rmSync(temporary, { recursive: true, force: true });
}
