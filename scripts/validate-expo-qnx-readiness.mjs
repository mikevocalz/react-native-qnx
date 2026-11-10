#!/usr/bin/env node
import { readFile, lstat } from 'node:fs/promises';
import { resolve, relative, isAbsolute, sep, win32 } from 'node:path';
import { fileURLToPath } from 'node:url';

const repositoryRoot = resolve(fileURLToPath(new URL('..', import.meta.url)));
const coreGateIds = [
  'external-app',
  'autolinking',
  'router',
  'dev-loop',
  'input-gestures',
  'worklets',
  'native-services',
  'distribution',
  'performance',
];
const optionalGateIds = ['nitro-forge'];
/** @param {string} message */
const fail = (message) => {
  throw new Error(message);
};

// Structural validation only. Human review must establish actual target proof.
export async function validateReadiness(root = repositoryRoot) {
  const manifest = JSON.parse(
    await readFile(resolve(root, 'docs/expo-delivery/qnx-readiness-gates.json'), 'utf8')
  );
  if (
    !manifest ||
    manifest.schemaVersion !== 1 ||
    manifest.target !== 'qnx' ||
    !Array.isArray(manifest.requirements) ||
    typeof manifest.referenceApplication !== 'string' ||
    !manifest.referenceApplication.trim()
  ) {
    fail('Invalid Expo QNX readiness manifest or referenceApplication');
  }
  const ids = new Set();
  const lines = [];
  let pending = 0;
  let optionalPending = 0;
  for (const gate of manifest.requirements) {
    if (!gate || typeof gate.id !== 'string' || ids.has(gate.id)) {
      fail('Invalid or duplicate gate id');
    }
    ids.add(gate.id);
    const optional = optionalGateIds.includes(gate.id);
    if (!optional && !coreGateIds.includes(gate.id)) fail('Unknown gate: ' + gate.id);
    if (gate.optional !== optional) fail(gate.id + ': invalid optional classification');
    for (const field of ['area', 'implementation', 'acceptance']) {
      if (typeof gate[field] !== 'string' || !gate[field].trim()) {
        fail(gate.id + ': missing ' + field);
      }
    }
    if (!['pending', 'passed'].includes(gate.status)) fail(gate.id + ': invalid status');
    if (gate.status === 'passed') {
      if (typeof gate.evidence !== 'string' || !gate.evidence.trim()) {
        fail(gate.id + ': cannot pass without an evidence path');
      }
      // Keep repository paths portable: reject Windows absolute paths on POSIX too.
      if (isAbsolute(gate.evidence) || win32.isAbsolute(gate.evidence)) {
        fail(gate.id + ': absolute evidence path refused');
      }
      if (gate.evidence.includes('\\') || gate.evidence.includes('\0')) {
        fail(gate.id + ': evidence must use a portable repository path');
      }
      const evidence = resolve(root, gate.evidence);
      const rel = relative(root, evidence);
      if (!rel || rel === '..' || rel.startsWith('..' + sep) || isAbsolute(rel)) {
        fail(gate.id + ': evidence escapes repository');
      }
      let current = root;
      for (const segment of rel.split(sep)) {
        current = resolve(current, segment);
        const stat = await lstat(current).catch(() =>
          fail(gate.id + ': evidence not found: ' + rel)
        );
        if (stat.isSymbolicLink()) fail(gate.id + ': symlink evidence refused');
      }
      const stat = await lstat(evidence);
      if (!stat.isFile()) fail(gate.id + ': evidence must be a file');
      if (!stat.size || !(await readFile(evidence, 'utf8')).trim()) {
        fail(gate.id + ': evidence must not be empty');
      }
    } else {
      if (gate.evidence !== null) fail(gate.id + ': pending gate must use null evidence');
      if (optional) optionalPending++;
      else pending++;
    }
    lines.push(
      (gate.status === 'passed' ? 'PASS' : 'PENDING') +
        '  ' +
        gate.id +
        (optional ? ' (optional)' : '') +
        ' — ' +
        gate.area
    );
  }
  if ([...coreGateIds, ...optionalGateIds].some((id) => !ids.has(id))) {
    fail('The Expo readiness inventory must retain every required and optional gate');
  }
  return {
    lines,
    pending,
    optionalPending,
    coreTotal: coreGateIds.length,
    optionalTotal: optionalGateIds.length,
  };
}

export async function main(args = process.argv.slice(2)) {
  if (
    args.some((arg) => !['--strict', '--include-optional'].includes(arg)) ||
    new Set(args).size !== args.length ||
    (args.includes('--include-optional') && !args.includes('--strict'))
  ) {
    console.error(
      'Usage: node scripts/validate-expo-qnx-readiness.mjs [--strict [--include-optional]]'
    );
    return 2;
  }
  try {
    const result = await validateReadiness();
    for (const line of result.lines) console.log(line);
    console.log(
      `Expo QNX readiness: ${result.coreTotal - result.pending}/${result.coreTotal} core passed, ` +
        `${result.pending} core pending; ${result.optionalPending}/${result.optionalTotal} optional pending`
    );
    console.log(
      'NOTE: File presence is not proof or a commit check; reviewers must verify committed evidence, device and artifact identity.'
    );
    return args.includes('--strict') &&
      (result.pending || (args.includes('--include-optional') && result.optionalPending))
      ? 1
      : 0;
  } catch (error) {
    console.error(
      'Expo QNX readiness: ' + (error instanceof Error ? error.message : String(error))
    );
    return 1;
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  process.exitCode = await main();
}
