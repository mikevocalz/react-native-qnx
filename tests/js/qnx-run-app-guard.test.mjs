// The guarded launcher (template/qnx/run-app.sh) on the host: a fake pidin lists
// fake processes and a fake app records how it was called. One instance per
// app even when the app's own socket is unreachable.
import assert from 'node:assert/strict';
import { spawn, spawnSync } from 'node:child_process';
import { chmod, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { desktopEntry } from '../../packages/react-native-qnx-cli/src/desktop-entry.mjs';

const descriptor = {
  appId: 'org.rnqnx.showcase',
  displayName: 'QNX Components',
  icon: { sizes: [{ size: 128 }] },
};
const script = desktopEntry(descriptor, { appDir: '/data/home/qnxuser/app' }).launcherScript;
assert.ok(!/kill\s+-(9|KILL|SIGKILL)|-s\s+KILL/i.test(script), 'the launcher never sends SIGKILL');

const dir = await mkdtemp(path.join(tmpdir(), 'rnqnx-run-app-'));
const children = [];
try {
  const app = path.join(dir, 'app');
  const state = path.join(dir, 'state');
  await Promise.all([
    import('node:fs/promises').then((fs) => fs.mkdir(app)),
    import('node:fs/promises').then((fs) => fs.mkdir(state)),
  ]);
  await writeFile(path.join(app, 'run-app.sh'), script, { mode: 0o755 });
  // Fake app: records its arguments; --restore-only / --stop-running answer
  // from files the test sets (reachable = exit 0, unreachable = exit 75).
  await writeFile(
    path.join(app, 'org.rnqnx.showcase'),
    `#!/bin/sh
echo "$*" >> "${state}/calls"
case "$1" in
  --restore-only) [ -f "${state}/reachable" ] && exit 0; exit 75 ;;
  --stop-running) [ -f "${state}/obeys-quit" ] && kill $(cat "${state}/pids") 2>/dev/null; exit 0 ;;
esac
exit 0
`,
    { mode: 0o755 }
  );
  // Fake pidin: prints "pid name uid" for the pids in state/pids that are alive.
  const uid = spawnSync('id', ['-u']).stdout.toString().trim();
  await writeFile(
    path.join(dir, 'pidin'),
    `#!/bin/sh
echo "     pid name              uid"
[ -f "${state}/pids" ] || exit 0
while read -r pid name owner; do
  kill -0 "$pid" 2>/dev/null && echo "$pid $name $owner"
done < "${state}/pids"
`,
    { mode: 0o755 }
  );
  // Asynchronous: the fake processes are children of this test, and only the
  // event loop reaps one after it is killed (a zombie still answers kill -0).
  const run = (args, env = {}) =>
    new Promise((resolve) => {
      const child = spawn('sh', [path.join(app, 'run-app.sh'), ...args], {
        env: {
          ...process.env,
          RNQNX_PIDIN: path.join(dir, 'pidin'),
          RNQNX_LOG: path.join(dir, 'app.log'),
          RNQNX_TERM_WAIT: '2',
          ...env,
        },
        stdio: ['ignore', 'pipe', 'pipe'],
      });
      let stderr = '';
      child.stderr.on('data', (chunk) => (stderr += chunk));
      child.stdout.resume();
      child.on('exit', (status) => resolve({ status, stderr }));
    });
  const calls = async () =>
    existsSync(path.join(state, 'calls'))
      ? (await readFile(path.join(state, 'calls'), 'utf8')).trim().split('\n')
      : [];
  const reset = async () => {
    for (const f of ['calls', 'pids', 'reachable', 'obeys-quit'])
      await rm(path.join(state, f), { force: true });
  };
  /** A process that stays until signalled; `name` is what pidin reports. */
  const fake = async (name, owner = uid) => {
    const child = spawn('sleep', ['60'], { stdio: 'ignore' });
    children.push(child);
    await writeFile(path.join(state, 'pids'), `${child.pid} ${name} ${owner}\n`, { flag: 'a' });
    return child;
  };
  const exited = (child) =>
    new Promise((resolve) =>
      child.exitCode !== null || child.signalCode ? resolve() : child.once('exit', resolve)
    );

  // 1. Nothing running: the app is started with the arguments as given.
  await reset();
  {
    const r = await run(['--dev-server', 'http://1.2.3.4:8081']);
    assert.equal(r.status, 0, r.stderr);
  }
  assert.deepEqual(await calls(), ['--dev-server http://1.2.3.4:8081']);

  // 2. Running and reachable: restore, no second start.
  await reset();
  let old = await fake('./org.rnqnx.showcase');
  await writeFile(path.join(state, 'reachable'), '');
  assert.equal((await run([])).status, 0);
  assert.deepEqual(await calls(), ['--restore-only']);
  assert.equal(old.exitCode, null, 'restore leaves the running app alone');
  old.kill();
  await exited(old);

  // 3. Running but its socket is unreachable (stale, other login): the old
  // process gets SIGTERM, then one new copy starts.
  await reset();
  old = await fake('org.rnqnx.showcase.dev');
  assert.equal((await run(['--flag'])).status, 0);
  await exited(old);
  assert.equal(old.signalCode, 'SIGTERM');
  assert.deepEqual(await calls(), ['--restore-only --flag', '--flag']);

  // 4. --replace: the app is asked to quit first; a process that obeys is
  // never signalled by the launcher, the new copy starts with --replace.
  await reset();
  old = await fake('org.rnqnx.showcase');
  await writeFile(path.join(state, 'obeys-quit'), '');
  await writeFile(path.join(state, 'pids'), `${old.pid} org.rnqnx.showcase ${uid}\n`);
  assert.equal((await run(['--replace', '--dev-server', 'http://1.2.3.4:8081'])).status, 0);
  await exited(old);
  assert.deepEqual(await calls(), ['--stop-running', '--replace --dev-server http://1.2.3.4:8081']);

  // 5. --replace when the app ignores quit: SIGTERM, then start.
  await reset();
  old = await fake('org.rnqnx.showcase');
  assert.equal((await run(['--replace'])).status, 0);
  await exited(old);
  assert.equal(old.signalCode, 'SIGTERM');
  assert.deepEqual(await calls(), ['--stop-running', '--replace']);

  // 6. A process that ignores SIGTERM: fail, start nothing, never SIGKILL.
  await reset();
  const stubborn = spawn('sh', ['-c', 'trap "" TERM; sleep 60 & wait'], { stdio: 'ignore' });
  children.push(stubborn);
  await writeFile(path.join(state, 'pids'), `${stubborn.pid} org.rnqnx.showcase ${uid}\n`);
  const stuck = await run(['--replace']);
  assert.equal(stuck.status, 1);
  assert.match(stuck.stderr, /did not stop after SIGTERM/);
  assert.deepEqual(await calls(), ['--stop-running']);
  assert.equal(stubborn.exitCode, null);

  // 7. Other apps and other users' processes are not this app.
  await reset();
  const other = await fake('org.rnqnx.animshowcase');
  await fake('org.rnqnx.showcase', String(Number(uid) + 1));
  assert.equal((await run([])).status, 0);
  assert.deepEqual(await calls(), ['']);
} finally {
  for (const child of children) child.kill('SIGKILL');
  await rm(dir, { recursive: true, force: true });
}
console.log('qnx-run-app-guard: ok');
