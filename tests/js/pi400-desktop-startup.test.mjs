import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const fixture = fileURLToPath(new URL('../pi400-desktop-startup-fixtures.py', import.meta.url));
const result = spawnSync('python3', [fixture], {
  encoding: 'utf8',
  timeout: 60000,
  killSignal: 'SIGKILL',
  maxBuffer: 1024 * 1024,
});
assert.equal(result.error, undefined, String(result.error));
assert.equal(result.status, 0, result.stdout + result.stderr);
assert.match(result.stdout, /23 executed host fixtures passed; syntax passed/);
console.log(result.stdout.trim());
