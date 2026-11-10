#!/usr/bin/env node
import { readFile, lstat } from 'node:fs/promises';
import { resolve, relative, isAbsolute, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(fileURLToPath(new URL('..', import.meta.url)));
const strict = process.argv.includes('--strict');
if (process.argv.some((arg) => arg !== '--strict' && arg !== process.argv[0] && arg !== process.argv[1])) {
  console.error('Usage: node scripts/validate-expo-qnx-readiness.mjs [--strict]');
  process.exit(2);
}
const manifestPath = resolve(root, 'docs/expo-delivery/qnx-readiness-gates.json');
const fail = (message) => {
  throw new Error(message);
};
const read = (path) => readFile(path, 'utf8');
const manifest = JSON.parse(await read(manifestPath));
if (manifest.schemaVersion !== 1 || manifest.target !== 'qnx' || !Array.isArray(manifest.requirements)) {
  fail('Invalid Expo QNX readiness manifest');
}
const ids = new Set();
let pending = 0;
let passed = 0;
for (const gate of manifest.requirements) {
  if (!gate || typeof gate.id !== 'string' || !/^[a-z0-9-]+$/.test(gate.id) || ids.has(gate.id)) {
    fail('Invalid or duplicate gate id');
  }
  ids.add(gate.id);
  for (const field of ['area', 'implementation', 'acceptance']) {
    if (typeof gate[field] !== 'string' || !gate[field].trim()) {
      fail(gate.id + ': missing ' + field);
    }
  }
  if (!['pending', 'passed'].includes(gate.status)) fail(gate.id + ': invalid status');
  if (gate.status === 'passed') {
    if (typeof gate.evidence !== 'string' || !gate.evidence.trim()) {
      fail(gate.id + ': cannot pass without a committed evidence path');
    }
    if (isAbsolute(gate.evidence)) fail(gate.id + ': absolute evidence path refused');
    const evidence = resolve(root, gate.evidence);
    const rel = relative(root, evidence);
    if (!rel || rel === '..' || rel.startsWith('..' + sep) || isAbsolute(rel)) {
      fail(gate.id + ': evidence escapes repository');
    }
    const segments = rel.split(sep);
    let current = root;
    for (const segment of segments) {
      current = resolve(current, segment);
      const stat = await lstat(current).catch(() => fail(gate.id + ': evidence not found: ' + rel));
      if (stat.isSymbolicLink()) fail(gate.id + ': symlink evidence refused');
    }
    const stat = await lstat(evidence);
    if (!stat.isFile()) fail(gate.id + ': evidence must be a file');
    passed++;
  } else {
    if (gate.evidence !== null) fail(gate.id + ': pending gate must use null evidence');
    pending++;
  }
  console.log((gate.status === 'passed' ? 'PASS' : 'PENDING') + '  ' + gate.id + ' — ' + gate.area);
}
if (!ids.size) fail('No Expo readiness gates defined');
console.log('Expo QNX readiness: ' + passed + '/' + ids.size + ' passed, ' + pending + ' pending');
console.log('NOTE: File presence alone is not proof; reviewers must independently verify evidence, device and artifact identity.');
if (strict && pending) process.exitCode = 1;
