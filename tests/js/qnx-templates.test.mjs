import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { chmod, cp, mkdir, mkdtemp, readFile, realpath, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { gunzipSync } from 'node:zlib';
import {
  extractTemplate,
  loadLock,
  loadRegistry,
  packTemplate,
  packVerified,
  readTar,
  selectTemplate,
  writeTar,
} from '../../packages/react-native-qnx-cli/src/templates.mjs';

// Host test for H14 (versioned template tarballs) and the deploy-bundle command.
// Nothing here touches a QNX target; ssh and scp are stubbed to run locally.
const root = process.cwd();
const cli = path.join(root, 'packages/react-native-qnx-cli/src/cli.mjs');
const temporary = await realpath(await mkdtemp(path.join(tmpdir(), 'rnqnx-templates-')));
/** @param {string} workspace @param {string[]} args @param {NodeJS.ProcessEnv} [env] */
const run = (workspace, args, env = process.env) =>
  spawnSync(process.execPath, [cli, ...args], {
    cwd: workspace,
    env,
    encoding: 'utf8',
    timeout: 30000,
  });

try {
  // The lock pins every registered template: version and content hash.
  const registry = await loadRegistry();
  const lock = await loadLock();
  assert.deepEqual(Object.keys(registry).sort(), Object.keys(lock).sort());
  for (const name of Object.keys(registry)) {
    const first = await packVerified(name);
    const second = await packTemplate(name);
    assert.equal(first.contentSha256, second.contentSha256, 'deterministic content hash');
    assert.equal(first.contentSha256, lock[name].contentSha256);
    assert.equal(first.version, lock[name].version);
    assert.match(first.contentSha256, /^[0-9a-f]{64}$/);
    // The content hash is the hash of the tar inside the tarball.
    assert.equal(
      createHash('sha256').update(gunzipSync(first.tarball)).digest('hex'),
      first.contentSha256
    );
  }

  // Extraction returns the template files byte for byte.
  const qnx = await packVerified('qnx');
  const files = extractTemplate(qnx);
  assert.deepEqual(await readFile(path.join(root, 'template/qnx/index.js')), files.get('index.js'));
  assert.deepEqual([...extractTemplate(qnx, ['index.js']).keys()], ['index.js']);
  assert.throws(() => extractTemplate(qnx, ['missing.js']), /lacks missing\.js/);

  // A tarball whose bytes differ from the pinned content is refused.
  const tampered = { ...qnx, contentSha256: '0'.repeat(64) };
  assert.throws(() => extractTemplate(tampered), /content hash mismatch/);

  // Selector: defaults per command, explicit names, wrong-command and unknown names.
  assert.equal((await selectTemplate('init', undefined)).name, 'qnx');
  assert.equal((await selectTemplate('project-create', undefined)).name, 'qnx-router');
  assert.equal((await selectTemplate('init', 'qnx')).name, 'qnx');
  await assert.rejects(
    selectTemplate('init', 'qnx-router'),
    /Unknown template "qnx-router" for init\. Available: qnx/
  );
  await assert.rejects(selectTemplate('project-create', 'nope'), /Available: qnx-router/);

  // Tar reader refuses traversal, absolute names and truncation.
  for (const bad of ['../escape.js', '/abs.js', 'a/../b.js', 'a\\b.js'])
    assert.throws(() => writeTar([{ path: bad, data: Buffer.from('x') }]), /Unsafe template entry/);
  const good = writeTar([{ path: 'a.txt', data: Buffer.from('hello') }]);
  assert.equal(readTar(good).get('a.txt')?.toString(), 'hello');
  assert.throws(() => readTar(good.subarray(0, 300)), /truncated/);
  const corrupt = Buffer.from(good);
  corrupt[10] ^= 0xff;
  assert.throws(() => readTar(corrupt), /checksum/);

  // A template edited without a version bump is refused, and a bump plus lock fixes it.
  const copy = path.join(temporary, 'copy');
  await mkdir(copy);
  await cp(path.join(root, 'template'), path.join(copy, 'template'), { recursive: true });
  await writeFile(path.join(copy, 'template/qnx/README.md'), 'changed\n');
  await assert.rejects(packVerified('qnx', copy), /changed without a version bump/);
  const registryPath = path.join(copy, 'template/templates.json');
  const edited = JSON.parse(await readFile(registryPath, 'utf8'));
  edited.templates.qnx.version = '1.0.1';
  await writeFile(registryPath, JSON.stringify(edited));
  await assert.rejects(packVerified('qnx', copy), /pins 1\.0\.0/);

  // CLI: init prints the version and hash; --template selects; a wrong name fails.
  const workspace = path.join(temporary, 'ws');
  await mkdir(path.join(workspace, 'apps'), { recursive: true });
  await cp(path.join(root, 'package.json'), path.join(workspace, 'package.json'));
  await cp(path.join(root, 'CMakePresets.json'), path.join(workspace, 'CMakePresets.json'));
  await cp(path.join(root, 'template'), path.join(workspace, 'template'), { recursive: true });
  const init = run(workspace, ['init', 'dashboard', '--template', 'qnx']);
  assert.equal(init.status, 0, init.stderr);
  assert.match(
    init.stdout,
    new RegExp(`Template qnx 1\\.0\\.0, content sha256 ${lock.qnx.contentSha256}`)
  );
  assert.notEqual(run(workspace, ['init', 'other', '--template', 'qnx-router']).status, 0);
  const list = run(workspace, ['templates', 'pack', '--out', path.join(temporary, 'out')]);
  assert.equal(list.status, 0, list.stderr);
  const manifest = JSON.parse(
    await readFile(path.join(temporary, 'out/templates-manifest.json'), 'utf8')
  );
  assert.deepEqual(
    manifest.templates.map((/** @type {{template: string}} */ t) => t.template),
    ['qnx', 'qnx-router']
  );
  const tgz = await readFile(path.join(temporary, 'out/qnx-1.0.0.tgz'));
  assert.equal(createHash('sha256').update(tgz).digest('hex'), manifest.templates[0].tarballSha256);

  // project create --dry-run reports the selected template.
  const dry = spawnSync(
    process.execPath,
    [
      path.join(root, 'packages/react-native-qnx-cli/src/bin.mjs'),
      'project',
      'create',
      '--project',
      path.join(temporary, 'p'),
      '--app-id',
      'org.example.notes',
      '--name',
      'Notes',
      '--dry-run',
    ],
    { cwd: root, encoding: 'utf8' }
  );
  assert.equal(dry.status, 0, dry.stderr);
  assert.equal(JSON.parse(dry.stdout).template.template, 'qnx-router');
  assert.equal(JSON.parse(dry.stdout).template.contentSha256, lock['qnx-router'].contentSha256);

  // deploy-bundle: fake hermesc and a local ssh/scp pair stand in for the target.
  const bin = path.join(temporary, 'bin');
  await mkdir(bin);
  const target = path.join(temporary, 'target');
  await mkdir(target);
  await writeFile(path.join(bin, 'ssh'), '#!/bin/bash\neval "${@: -1}"\n');
  await writeFile(
    path.join(bin, 'scp'),
    '#!/bin/bash\nsrc="${@: -2:1}"; dst="${@: -1}"; cp "$src" "${dst#*:}"\n'
  );
  const hermesc = path.join(bin, 'hermesc');
  await writeFile(
    hermesc,
    '#!/bin/bash\nwhile [[ $# -gt 0 ]]; do if [[ $1 == -out ]]; then out=$2; shift; elif [[ $1 != -* ]]; then in=$1; fi; shift; done\n{ printf "HBC"; cat "$in"; } > "$out"\n'
  );
  for (const tool of ['ssh', 'scp', 'hermesc']) await chmod(path.join(bin, tool), 0o755);
  await mkdir(path.join(workspace, 'scripts/lib'), { recursive: true });
  await cp(
    path.join(root, 'scripts/deploy-bundle.sh'),
    path.join(workspace, 'scripts/deploy-bundle.sh')
  );
  await cp(
    path.join(root, 'scripts/lib/qnx-target.sh'),
    path.join(workspace, 'scripts/lib/qnx-target.sh')
  );
  await mkdir(path.join(workspace, 'build'));
  await writeFile(path.join(workspace, 'build/dashboard.bundle.js'), 'console.log(1);\n');
  const env = {
    ...process.env,
    PATH: `${bin}:${process.env.PATH}`,
    QNX_TARGET_DIR: target,
    QNX_TARGET_IP: 'qnx.example.test',
    QNX_TARGET_USER: 'dev',
  };
  const plan = run(
    workspace,
    ['deploy-bundle', '--app', 'dashboard', '--hermesc', hermesc, '--dry-run'],
    env
  );
  assert.equal(plan.status, 0, plan.stderr);
  const steps = JSON.parse(plan.stdout).steps;
  assert.deepEqual(
    steps.map((/** @type {{step: string}} */ s) => s.step),
    ['hermesc', 'push']
  );
  assert.deepEqual(steps[0].args.slice(0, 2), ['-O', '-emit-binary']);
  const deployed = run(
    workspace,
    ['deploy-bundle', '--app', 'dashboard', '--hermesc', hermesc, '--run', '--runner', 'runner.sh'],
    {
      ...env,
      PATH: env.PATH,
    }
  );
  // The runner script does not exist yet: the push must still land and the run must fail loudly.
  assert.notEqual(deployed.status, 0);
  const pushed = await readFile(path.join(target, 'bundles/dashboard.hbc'), 'utf8');
  assert.equal(pushed, 'HBCconsole.log(1);\n');
  assert.match(deployed.stdout, /Target sha256 matches|only the size/);
  await writeFile(path.join(target, 'runner.sh'), '#!/bin/bash\necho "ran with $*"\n');
  await chmod(path.join(target, 'runner.sh'), 0o755);
  const ran = run(
    workspace,
    ['deploy-bundle', '--app', 'dashboard', '--hermesc', hermesc, '--run', '--runner', 'runner.sh'],
    env
  );
  assert.equal(ran.status, 0, ran.stderr + ran.stdout);
  assert.match(ran.stdout, /ran with --bundle .*bundles\/dashboard\.hbc/);
  // Missing bundle, bad app name, --runner without --run: refused before any push.
  assert.match(
    run(workspace, ['deploy-bundle', '--app', 'nope', '--hermesc', hermesc], env).stderr,
    /bundle-qnx --app nope first/
  );
  assert.notEqual(run(workspace, ['deploy-bundle', '--app', 'Bad_Name'], env).status, 0);
  assert.match(
    run(workspace, ['deploy-bundle', '--app', 'dashboard', '--runner', 'x'], env).stderr,
    /--runner only applies together with --run/
  );
} finally {
  await rm(temporary, { recursive: true, force: true });
}
console.log('qnx templates and deploy-bundle tests passed');
