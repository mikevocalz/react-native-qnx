// The JS runner imports tests in one process. Isolate Module._load and router
// caches so this test cannot retain its React Native mock in later cases.
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const output = execFileSync(
  process.execPath,
  [fileURLToPath(new URL('./fixtures/drawer-settlement.mjs', import.meta.url))],
  { cwd: process.cwd(), encoding: 'utf8' }
);
console.log(output.trim());
