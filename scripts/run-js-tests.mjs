#!/usr/bin/env node

import { readdir } from 'node:fs/promises';
import path from 'node:path';

async function main() {
  const testDir = path.join(process.cwd(), 'tests', 'js');

  try {
    const entries = await readdir(testDir);
    const testFiles = entries.filter((f) => f.endsWith('.test.mjs') || f.endsWith('.test.ts'));

    if (testFiles.length === 0) {
      console.log('No JavaScript tests found in tests/js. Skipping.');
      process.exitCode = 0;
      return;
    }

    for (const file of testFiles) {
      const filePath = path.join(testDir, file);
      console.log(`Running ${filePath}...`);
      await import(filePath);
    }

    console.log('All JavaScript tests passed.');
    process.exitCode = 0;
  } catch (err) {
    if (
      err instanceof Error &&
      'code' in err &&
      /** @type {NodeJS.ErrnoException} */ (err).code === 'ENOENT'
    ) {
      console.log('tests/js directory does not exist. Skipping.');
      process.exitCode = 0;
      return;
    }
    console.error(err);
    process.exitCode = 1;
  }
}

await main();
