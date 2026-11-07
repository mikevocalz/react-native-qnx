#!/usr/bin/env node
import { readFile, stat } from 'node:fs/promises';
import path from 'node:path';
import { qualify, verifyArtifacts } from './lib/qualification.mjs';

const args = process.argv.slice(2);
try {
  const requireComplete = args.includes('--require-complete');
  const files = args.filter((arg) => arg !== '--require-complete');
  if (files.length !== 1 || files[0].startsWith('-') || args.length !== (requireComplete ? 2 : 1)) {
    throw new Error(
      'Usage: node scripts/qualify-qnx-evidence.mjs <capture.json> [--require-complete]'
    );
  }
  const input = path.resolve(files[0]);
  const size = await stat(input);
  if (!size.isFile() || size.size > 4 * 1024 * 1024) {
    throw new Error('Capture JSON must be a file of at most 4 MiB.');
  }
  const document = JSON.parse(await readFile(input, 'utf8'));
  const report = qualify(document);
  const integrity = await verifyArtifacts(document, path.dirname(input));
  console.log(JSON.stringify({ ...report, integrity }, null, 2));
  process.exitCode = requireComplete && !report.captureComplete ? 2 : 0;
} catch (error) {
  console.error(`rnqnx qualification: ${error instanceof Error ? error.message : error}`);
  process.exitCode = 1;
}
