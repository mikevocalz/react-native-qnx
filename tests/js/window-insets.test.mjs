// The ordered runner imports tests in one process. Keep this mounted React
// fixture in a fresh Node process so earlier Module._load mocks cannot replace
// its React implementation or retain its native modules in later tests.
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const output = execFileSync(
  process.execPath,
  [fileURLToPath(new URL('./fixtures/window-insets.mjs', import.meta.url))],
  { cwd: process.cwd(), encoding: 'utf8', timeout: 60_000 }
);
console.log(output.trim());
