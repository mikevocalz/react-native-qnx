import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import {
  copyFileSync,
  existsSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  realpathSync,
  rmSync,
  symlinkSync,
  writeFileSync,
} from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';

// Workflow fixture: real git clone/checkout and npm ci/build commands, tiny
// dependency-free site. This does not substitute for a real Next.js site build.
const temp = mkdtempSync(join(realpathSync(tmpdir()), 'docs-freeze-fixture-'));
/** @param {string} command @param {string[]} args @param {string} cwd */
function run(command, args, cwd) {
  return spawnSync(command, args, {
    cwd,
    encoding: 'utf8',
    timeout: 60000,
    env: {
      ...process.env,
      npm_config_update_notifier: 'false',
      npm_config_cache: join(temp, 'cache'),
    },
  });
}
/** @param {string} command @param {string[]} args @param {string} cwd */
function checked(command, args, cwd) {
  const result = run(command, args, cwd);
  assert.equal(result.status, 0, result.stderr);
  return result.stdout.trim();
}
try {
  const repo = join(temp, 'repo'),
    site = join(repo, 'docs-site'),
    script = join(site, 'scripts/freeze-candidate.mjs');
  mkdirSync(join(site, 'scripts'), { recursive: true });
  copyFileSync(resolve('docs-site/scripts/freeze-candidate.mjs'), script);
  const pkg = {
    name: 'snapshot-fixture',
    version: '1.0.0',
    scripts: {
      'data:check': 'node -e "if(process.env.DOCS_SOURCE_COMMIT) process.exit(9)"',
      data: "node -e \"require('node:fs').writeFileSync('frozen-source.txt',process.env.DOCS_SOURCE_COMMIT)\"",
      build: 'node build.mjs',
      'check:content': "node -e \"require('node:fs').accessSync('out/docs.html')\"",
    },
  };
  writeFileSync(join(site, 'package.json'), JSON.stringify(pkg));
  writeFileSync(
    join(site, 'package-lock.json'),
    JSON.stringify({
      name: pkg.name,
      version: pkg.version,
      lockfileVersion: 3,
      requires: true,
      packages: { '': { name: pkg.name, version: pkg.version } },
    })
  );
  writeFileSync(
    join(site, 'build.mjs'),
    "import {mkdirSync,writeFileSync,readFileSync} from 'node:fs'; mkdirSync('out'); writeFileSync('out/docs.html', JSON.stringify({text:readFileSync('page.txt','utf8'),base:process.env.DOCS_BASE_PATH,commit:process.env.DOCS_SOURCE_COMMIT,generatedCommit:readFileSync('frozen-source.txt','utf8')}));\n"
  );
  writeFileSync(join(site, 'page.txt'), 'committed page');
  checked('git', ['init', '--quiet'], repo);
  checked('git', ['add', '.'], repo);
  checked(
    'git',
    [
      '-c',
      'user.name=Snapshot Fixture',
      '-c',
      'user.email=fixture@example.invalid',
      'commit',
      '--quiet',
      '-m',
      'fixture',
    ],
    repo
  );
  const commit = checked('git', ['rev-parse', 'HEAD'], repo);
  writeFileSync(join(site, 'page.txt'), 'uncommitted edit must not leak');
  const output = join(temp, 'frozen');
  checked(process.execPath, [script, commit, output, '/project'], repo);
  const manifest = JSON.parse(readFileSync(join(output, 'snapshot.json'), 'utf8'));
  assert.equal(manifest.kind, 'candidate');
  assert.equal(manifest.sourceCommit, commit);
  assert.equal(manifest.basePath, `/project/v/candidate-${commit.slice(0, 12)}`);
  const page = readFileSync(join(output, 'docs.html'));
  assert.deepEqual(JSON.parse(page.toString('utf8')), {
    text: 'committed page',
    base: manifest.basePath,
    commit,
    generatedCommit: commit,
  });
  assert.equal(manifest.files['docs.html'], createHash('sha256').update(page).digest('hex'));
  assert.equal(
    manifest.docsLockSha256,
    createHash('sha256')
      .update(readFileSync(join(site, 'package-lock.json')))
      .digest('hex')
  );
  assert.notEqual(run(process.execPath, [script, commit, output], repo).status, 0);
  assert.deepEqual(readFileSync(join(output, 'docs.html')), page, 'existing snapshot preserved');
  const linked = join(temp, 'linked');
  symlinkSync(join(temp, 'absent'), linked);
  assert.notEqual(run(process.execPath, [script, commit, linked], repo).status, 0);
  assert.notEqual(
    run(process.execPath, [script, commit, join(temp, 'unsafe'), '/x/../y'], repo).status,
    0
  );
  assert.equal(existsSync(join(temp, 'unsafe')), false);
  writeFileSync(join(site, 'build.mjs'), 'process.exit(7);\n');
  checked('git', ['add', '.'], repo);
  checked(
    'git',
    [
      '-c',
      'user.name=Snapshot Fixture',
      '-c',
      'user.email=fixture@example.invalid',
      'commit',
      '--quiet',
      '-m',
      'failing build',
    ],
    repo
  );
  const failed = join(temp, 'failed');
  const result = run(process.execPath, [script, 'HEAD', failed], repo);
  assert.notEqual(result.status, 0);
  assert.match(result.stderr, /npm run build failed/);
  assert.equal(existsSync(failed), false);
  assert.equal(existsSync(`${failed}.freeze-lock`), false);
  console.log(
    'docs candidate snapshot: committed-source isolation, frozen path/hash metadata, immutable output and failure cleanup pass (synthetic site)'
  );
} finally {
  rmSync(temp, { recursive: true, force: true });
}
