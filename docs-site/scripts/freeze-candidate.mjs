#!/usr/bin/env node
// Build an immutable documentation preview from one committed source revision.
import { spawnSync } from 'node:child_process';
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
import { tmpdir } from 'node:os';
import { basename, dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const repo = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const [revision, destination, deployBase = '/react-native-qnx'] = process.argv.slice(2);
if (!revision || !destination || process.argv.length > 5) {
  console.error(
    'Usage: node docs-site/scripts/freeze-candidate.mjs <commit-or-ref> <new-output-directory> [deploy-base]'
  );
  process.exit(1);
}
if (deployBase && !/^\/[a-zA-Z0-9/_-]+$/.test(deployBase))
  throw new Error('deploy-base must be an absolute URL path without dot segments');
function run(command, args, cwd, env = process.env) {
  if (command === 'npm') console.error(`Running npm ${args.join(' ')}`);
  const result = spawnSync(command, args, {
    cwd,
    env,
    encoding: 'utf8',
    stdio: ['ignore', 'pipe', 'pipe'],
  });
  if (result.status !== 0)
    throw new Error(
      `${command} ${args.join(' ')} failed: ${result.error?.message ?? `${result.stdout}\n${result.stderr}`}`
    );
  return result.stdout.trim();
}
const commit = run(
  'git',
  ['rev-parse', '--verify', '--end-of-options', `${revision}^{commit}`],
  repo
);
if (!/^[0-9a-f]{40}$/.test(commit)) throw new Error('Expected a full Git commit');
const id = `candidate-${commit.slice(0, 12)}`;
// Resolve the existing parent once, including system aliases such as /tmp.
const out = join(realpathSync(dirname(resolve(destination))), basename(resolve(destination)));
function assertAbsent() {
  try {
    lstatSync(out);
  } catch (error) {
    if (error.code === 'ENOENT') return;
    throw error;
  }
  throw new Error(`Output already exists: ${out}`);
}
assertAbsent();
const lock = `${out}.freeze-lock`;
mkdirSync(lock);
let temp, staged;
try {
  temp = mkdtempSync(join(process.env.DOCS_SNAPSHOT_WORK ?? tmpdir(), 'rnqnx-docs-candidate-'));
  const clone = join(temp, 'source');
  run('git', ['clone', '--no-hardlinks', '--no-checkout', repo, clone], repo);
  run('git', ['checkout', '--detach', commit], clone);
  const site = join(clone, 'docs-site');
  const basePath = `${deployBase.replace(/\/$/, '')}/v/${id}`;
  const env = { ...process.env, DOCS_BASE_PATH: basePath, DOCS_SOURCE_COMMIT: commit };
  run('npm', ['ci', '--no-audit', '--no-fund'], site, env);
  run('npm', ['run', 'data:check'], site, { ...env, DOCS_SOURCE_COMMIT: '' });
  run('npm', ['run', 'data'], site, env);
  run('npm', ['run', 'build'], site, env);
  run('npm', ['run', 'check:content'], site, env);
  const built = join(site, 'out');
  if (!existsSync(join(built, 'docs.html')))
    throw new Error('No exported documentation entry point');
  if (existsSync(join(built, 'snapshot.json')))
    throw new Error('The export already contains the reserved snapshot.json receipt');
  const files = {};
  function inventory(folder, prefix = '') {
    for (const name of readdirSync(folder).sort()) {
      const path = join(folder, name),
        relative = `${prefix}${name}`,
        stat = lstatSync(path);
      if (stat.isSymbolicLink()) throw new Error(`Refusing export symlink: ${relative}`);
      if (stat.isDirectory()) inventory(path, `${relative}/`);
      else if (stat.isFile())
        files[relative] = createHash('sha256').update(readFileSync(path)).digest('hex');
      else throw new Error(`Unsupported export entry: ${relative}`);
    }
  }
  inventory(built);
  mkdirSync(dirname(out), { recursive: true });
  staged = mkdtempSync(join(dirname(out), '.docs-candidate-'));
  cpSync(built, staged, { recursive: true });
  writeFileSync(
    join(staged, 'snapshot.json'),
    JSON.stringify(
      {
        schema: 1,
        kind: 'candidate',
        id,
        sourceCommit: commit,
        basePath,
        docsLockSha256: createHash('sha256')
          .update(readFileSync(join(site, 'package-lock.json')))
          .digest('hex'),
        checks: [
          'npm ci',
          'npm run data:check',
          'npm run data',
          'npm run build',
          'npm run check:content',
        ],
        files,
      },
      null,
      2
    ) + '\n'
  );
  // Recheck before publishing the completed directory. Existing snapshots stay untouched.
  assertAbsent();
  renameSync(staged, out);
  staged = undefined;
  console.log(
    JSON.stringify({
      id,
      sourceCommit: commit,
      basePath,
      output: out,
      files: Object.keys(files).length,
    })
  );
} finally {
  if (staged) rmSync(staged, { recursive: true, force: true });
  if (temp) rmSync(temp, { recursive: true, force: true });
  rmSync(lock, { recursive: true, force: true });
}
