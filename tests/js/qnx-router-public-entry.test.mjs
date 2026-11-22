import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
const output = execFileSync(
  process.execPath,
  [fileURLToPath(new URL('./fixtures/router-public-entry.mjs', import.meta.url))],
  {
    cwd: process.cwd(),
    encoding: 'utf8',
    timeout: 60_000,
  }
);
console.log(output.trim());
