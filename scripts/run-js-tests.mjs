#!/usr/bin/env node

import { readdir } from 'node:fs/promises';
import path from 'node:path';

async function main() {
  const testDir = path.join(process.cwd(), 'tests', 'js');

  try {
    const entries = await readdir(testDir);
    const testFiles = entries
      .filter((f) => f.endsWith('.test.mjs') || f.endsWith('.test.ts'))
      .sort();

    if (testFiles.length === 0) {
      throw new Error('No JavaScript tests found in tests/js.');
    }

    for (const file of testFiles) {
      const filePath = path.join(testDir, file);
      console.log(`Running ${filePath}...`);
      await import(filePath);
    }

    console.log('All JavaScript tests passed.');
    process.exitCode = 0;
  } catch (err) {
    console.error(err);
    process.exitCode = 1;
  }
}

await main();
