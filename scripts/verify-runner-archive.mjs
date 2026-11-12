#!/usr/bin/env node
// Check a runner archive against a local rnqnx-runner-manifest/v1 file before
// it is attested and published (ADR-0018). The archive must match one entry's
// size and SHA-256, and must pass the same content policy `rnqnx runner fetch`
// applies: relative paths only, and no QNX SDP or target-image files in a QNX
// runner. Prints the matching entry id.
import { createHash } from 'node:crypto';
import { createReadStream } from 'node:fs';
import { readFile, stat } from 'node:fs/promises';
import {
  checkRunnerContents,
  listTarGz,
  validateRunnerManifest,
} from '../packages/react-native-qnx-cli/src/runner.mjs';

const [manifestFile, archive] = process.argv.slice(2);
if (!manifestFile || !archive) {
  console.error('Usage: node scripts/verify-runner-archive.mjs <manifest.json> <archive.tar.gz>');
  process.exit(2);
}
try {
  const manifest = validateRunnerManifest(JSON.parse(await readFile(manifestFile, 'utf8')));
  const hash = createHash('sha256');
  for await (const chunk of createReadStream(archive)) hash.update(chunk);
  const sha256 = hash.digest('hex');
  const { size } = await stat(archive);
  const entry = manifest.runners.find((r) => r.sha256 === sha256);
  if (!entry) throw new Error(`No manifest entry has SHA-256 ${sha256}.`);
  if (entry.size !== size)
    throw new Error(`${entry.id}: archive is ${size} bytes; the manifest says ${entry.size}.`);
  const names = await listTarGz(archive);
  checkRunnerContents(entry, names);
  console.log(
    JSON.stringify({ id: entry.id, target: entry.target, sha256, size, entries: names.length })
  );
} catch (error) {
  console.error(error instanceof Error ? error.message : error);
  process.exit(1);
}
