#!/usr/bin/env node
// Assemble a reviewable Pages artifact; never call GitHub or deploy it.
import { createHash } from 'node:crypto';
import {
  cpSync,
  existsSync,
  lstatSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  readdirSync,
  realpathSync,
  renameSync,
  rmSync,
  writeFileSync,
} from 'node:fs';
import { basename, dirname, isAbsolute, join, relative, resolve, sep } from 'node:path';

const [
  liveArgument,
  candidateArgument,
  expectedReceiptSha,
  outputArgument,
  siteBase = '/react-native-qnx',
] = process.argv.slice(2);
if (
  !liveArgument ||
  !candidateArgument ||
  !expectedReceiptSha ||
  !outputArgument ||
  process.argv.length > 7
)
  throw new Error(
    'Usage: node assemble-published.mjs <live-export> <candidate-export> <expected-snapshot-json-sha256> <new-output> [site-base]'
  );
if (!/^[a-f0-9]{64}$/.test(expectedReceiptSha))
  throw new Error('Expected an independently recorded snapshot.json SHA-256');
if (siteBase && !/^\/[A-Za-z0-9/_-]+$/.test(siteBase)) throw new Error('Invalid site base path');
const live = realpathSync(liveArgument),
  candidate = realpathSync(candidateArgument);
if (
  lstatSync(resolve(liveArgument)).isSymbolicLink() ||
  lstatSync(resolve(candidateArgument)).isSymbolicLink()
)
  throw new Error('Input root symlink refused');
const output = join(
  realpathSync(dirname(resolve(outputArgument))),
  basename(resolve(outputArgument))
);
function requireAbsent(path) {
  try {
    lstatSync(path);
  } catch (error) {
    if (error.code === 'ENOENT') return;
    throw error;
  }
  throw new Error(`Existing output refused: ${path}`);
}
function digest(path) {
  return createHash('sha256').update(readFileSync(path)).digest('hex');
}
function safeName(name) {
  if (
    !name ||
    name.includes('\\') ||
    name.split('/').some((part) => !part || part === '.' || part === '..') ||
    isAbsolute(name)
  )
    throw new Error(`Unsafe export path: ${name}`);
}
function inventory(root, prefix = '', result = new Map()) {
  for (const name of readdirSync(root).sort()) {
    const path = join(root, name),
      rel = `${prefix}${name}`,
      stat = lstatSync(path);
    safeName(rel);
    if (stat.isSymbolicLink()) throw new Error(`Export symlink refused: ${rel}`);
    if (stat.isDirectory()) inventory(path, `${rel}/`, result);
    else if (stat.isFile()) result.set(rel, digest(path));
    else throw new Error(`Unsupported export entry: ${rel}`);
  }
  return result;
}
const receiptPath = join(candidate, 'snapshot.json');
if (lstatSync(receiptPath).isSymbolicLink() || digest(receiptPath) !== expectedReceiptSha)
  throw new Error('Candidate receipt hash mismatch');
const receipt = JSON.parse(readFileSync(receiptPath, 'utf8'));
if (
  receipt.schema !== 1 ||
  receipt.kind !== 'candidate' ||
  typeof receipt.sourceCommit !== 'string' ||
  !/^[a-f0-9]{40}$/.test(receipt.sourceCommit) ||
  receipt.id !== `candidate-${receipt.sourceCommit.slice(0, 12)}`
)
  throw new Error('Invalid candidate identity');
if (receipt.basePath !== `${siteBase.replace(/\/$/, '')}/v/${receipt.id}`)
  throw new Error('Candidate was built for another deploy path');
const liveFiles = inventory(live),
  candidateFiles = inventory(candidate);
if (!liveFiles.has('index.html') || !liveFiles.has('docs.html') || !candidateFiles.has('docs.html'))
  throw new Error('Expected exported site entry points');
const claimed = new Map(Object.entries(receipt.files ?? {}));
if (claimed.size + 1 !== candidateFiles.size) throw new Error('Candidate inventory is incomplete');
for (const [name, sha] of claimed) {
  safeName(name);
  if (
    name === 'snapshot.json' ||
    !/^[a-f0-9]{64}$/.test(String(sha)) ||
    candidateFiles.get(name) !== sha
  )
    throw new Error(`Candidate file hash mismatch: ${name}`);
}
for (const root of [live, candidate]) {
  const rel = relative(root, output);
  if (!rel || (rel !== '..' && !rel.startsWith(`..${sep}`) && !isAbsolute(rel)))
    throw new Error('Output cannot modify an input tree');
}
requireAbsent(output);
requireAbsent(`${output}.assembly.json`);
const lock = `${output}.assembly-lock`;
mkdirSync(lock);
let staging;
try {
  staging = mkdtempSync(join(dirname(output), '.pages-assembly-'));
  cpSync(live, staging, { recursive: true });
  const destination = join(staging, 'v', receipt.id);
  if (existsSync(destination)) {
    const existing = inventory(destination);
    if (
      existing.size !== candidateFiles.size ||
      [...candidateFiles].some(([name, sha]) => existing.get(name) !== sha)
    )
      throw new Error('Immutable snapshot collision');
  } else {
    mkdirSync(dirname(destination), { recursive: true });
    cpSync(candidate, destination, { recursive: true });
  }
  // Source changes or an accidental overlap must never replace a live byte.
  const assembled = inventory(staging);
  for (const [name, sha] of liveFiles)
    if (assembled.get(name) !== sha) throw new Error(`Live file changed: ${name}`);
  for (const [name, sha] of candidateFiles)
    if (assembled.get(`v/${receipt.id}/${name}`) !== sha)
      throw new Error(`Copied candidate changed: ${name}`);
  for (const [root, original] of [
    [live, liveFiles],
    [candidate, candidateFiles],
  ]) {
    const after = inventory(root);
    if (
      after.size !== original.size ||
      [...original].some(([name, sha]) => after.get(name) !== sha)
    )
      throw new Error('Input changed during assembly');
  }
  requireAbsent(output);
  renameSync(staging, output);
  staging = undefined;
  const result = {
    schema: 1,
    deployed: false,
    snapshotId: receipt.id,
    sourceCommit: receipt.sourceCommit,
    snapshotReceiptSha256: expectedReceiptSha,
    preservedLiveFiles: Object.fromEntries(liveFiles),
    candidateFiles: Object.fromEntries(candidateFiles),
  };
  writeFileSync(`${output}.assembly.json`, JSON.stringify(result, null, 2) + '\n', { flag: 'wx' });
  console.log(
    JSON.stringify({
      output,
      snapshotId: receipt.id,
      preservedLiveFiles: liveFiles.size,
      candidateFiles: candidateFiles.size,
      deployed: false,
    })
  );
} finally {
  if (staging) rmSync(staging, { recursive: true, force: true });
  rmSync(lock, { recursive: true, force: true });
}
