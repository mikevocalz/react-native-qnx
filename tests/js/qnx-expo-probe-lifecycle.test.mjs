// @ts-nocheck: subprocess integration fixture, isolated from the JS runner's mocks.
import assert from 'node:assert/strict';
import { spawn, execFile } from 'node:child_process';
import { promisify } from 'node:util';
import fs from 'node:fs/promises';
import net from 'node:net';
import os from 'node:os';
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import { setTimeout as delay } from 'node:timers/promises';

const temporary = await fs.mkdtemp(
  path.join(process.env.RNQNX_EVIDENCE_ROOT || os.tmpdir(), 'expo-probe-lifecycle-')
);
const source = path.resolve('apps/expo-router-example/app/index.js');
const original = await fs.readFile(source, 'utf8');
const processes = [];
const exec = promisify(execFile);
async function processRows() {
  const { stdout } = await exec('ps', ['-axo', 'pid=,ppid=,pgid=,stat=']);
  return stdout
    .trim()
    .split('\n')
    .map((line) => {
      const [pid, ppid, pgid, state] = line.trim().split(/\s+/);
      return { pid: Number(pid), ppid: Number(ppid), pgid: Number(pgid), state };
    });
}
function launch(out, extra = [], env = {}) {
  const child = spawn(
    process.execPath,
    ['scripts/probe-expo-qnx-dev-server.mjs', '--out', out, ...extra],
    {
      env: { ...process.env, ...env },
      stdio: ['ignore', 'pipe', 'pipe'],
    }
  );
  let output = '';
  child.stdout.on('data', (part) => {
    output += part;
  });
  child.stderr.on('data', (part) => {
    output += part;
  });
  const closed = new Promise((resolve, reject) => {
    child.once('error', reject);
    child.once('close', (code, signal) => resolve({ code, signal, output }));
  });
  const state = { child, closed };
  processes.push(state);
  return state;
}
async function finish(state, timeout = 15_000) {
  return await Promise.race([
    state.closed,
    delay(timeout, undefined, { ref: false }).then(() => {
      throw Error('Probe cleanup timed out');
    }),
  ]);
}
async function isListening(port) {
  return await new Promise((resolve) => {
    const socket = net.createConnection({ host: '127.0.0.1', port });
    socket.once('connect', () => {
      socket.destroy();
      resolve(true);
    });
    socket.once('error', () => {
      socket.destroy();
      resolve(false);
    });
    socket.setTimeout(1000, () => {
      socket.destroy();
      resolve(true);
    });
  });
}
try {
  const absentOut = path.join(temporary, 'absent-python');
  const absent = await finish(
    launch(absentOut, ['--python', path.join(temporary, 'missing-python')])
  );
  assert.notEqual(absent.code, 0);
  assert.match(absent.output, /ENOENT/);
  assert.equal(await fs.readFile(source, 'utf8'), original);
  assert.equal(await fs.stat(path.join(absentOut, 'result.json')).catch(() => null), null);

  // Inject a real post-spawn metadata failure, in the test subprocess only.
  // The supervisor is started before session.json becomes a directory.
  const preload = path.join(temporary, 'setup-failure.mjs');
  await fs.writeFile(
    preload,
    `import cp from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { syncBuiltinESMExports } from 'node:module';
const real = cp.spawn;
cp.spawn = function(command, args, options) {
  const child = real(command, args, options);
  if (args?.some((arg) => String(arg).endsWith('/guarded-run.py'))) {
    const out = process.env.RNQNX_PROBE_FIXTURE_OUT;
    const session = path.join(out, 'session.json');
    fs.copyFileSync(session, path.join(out, 'startup-session.json'));
    fs.unlinkSync(session); fs.mkdirSync(session);
  }
  return child;
};
syncBuiltinESMExports();
`
  );
  const brokenOut = path.join(temporary, 'setup-failure');
  const broken = await finish(
    launch(brokenOut, [], {
      NODE_OPTIONS: `${process.env.NODE_OPTIONS || ''} --import=${pathToFileURL(preload).href}`,
      RNQNX_PROBE_FIXTURE_OUT: brokenOut,
    })
  );
  assert.notEqual(broken.code, 0);
  assert.match(broken.output, /EISDIR/);
  const setupSession = JSON.parse(
    await fs.readFile(path.join(brokenOut, 'startup-session.json'), 'utf8')
  );
  assert.equal(await isListening(setupSession.port), false, 'startup-failure server leaked');
  assert.equal(await fs.stat(path.join(brokenOut, 'result.json')).catch(() => null), null);

  // A send alone is not proof of reload: SDK58 rejects another protocol
  // version. The actual observer must receive the server's v2 broadcast.
  const wrongProtocol = path.join(temporary, 'wrong-protocol.mjs');
  await fs.writeFile(
    wrongProtocol,
    `const original = WebSocket.prototype.send;
WebSocket.prototype.send = function(data) {
  let payload = data;
  try { const value = JSON.parse(String(data)); if (value.method === 'reload') payload = JSON.stringify({ ...value, version: 999 }); } catch {}
  return original.call(this, payload);
};
`
  );
  const rejectedOut = path.join(temporary, 'wrong-protocol');
  const rejected = await finish(
    launch(rejectedOut, [], {
      NODE_OPTIONS: `${process.env.NODE_OPTIONS || ''} --import=${pathToFileURL(wrongProtocol).href}`,
    }),
    180_000
  );
  assert.notEqual(rejected.code, 0);
  assert.match(rejected.output, /Timed out waiting for Expo host observation/);
  const rejectedSession = JSON.parse(
    await fs.readFile(path.join(rejectedOut, 'session.json'), 'utf8')
  );
  assert.equal(await isListening(rejectedSession.port), false, 'rejected-protocol server leaked');
  assert.equal(await fs.readFile(source, 'utf8'), original);
  assert.equal(await fs.stat(path.join(rejectedOut, 'result.json')).catch(() => null), null);

  const cancelOut = path.join(temporary, 'cancel');
  const cancelling = launch(cancelOut);
  let edited = false;
  let ownedMembers = [];
  let ownedGroup;
  const deadline = Date.now() + 120_000;
  while (Date.now() < deadline && cancelling.child.exitCode === null) {
    if ((await fs.readFile(source, 'utf8')).includes('RNQNX_EXPO_HOST_WATCH_CONTROL')) {
      const session = JSON.parse(await fs.readFile(path.join(cancelOut, 'session.json'), 'utf8'));
      const rows = await processRows();
      const leader = rows.find((row) => row.ppid === session.supervisorPid && row.pid === row.pgid);
      assert.ok(leader, 'supervisor did not own the Expo session leader');
      ownedGroup = leader.pgid;
      ownedMembers = rows.filter((row) => row.pgid === ownedGroup);
      edited = true;
      cancelling.child.kill('SIGINT');
      break;
    }
    await delay(10);
  }
  assert.ok(edited, 'did not observe the actual temporary source edit');
  const cancelled = await finish(cancelling);
  assert.notEqual(cancelled.code, 0);
  assert.equal(await fs.readFile(source, 'utf8'), original, 'source not restored on cancellation');
  const cancelSession = JSON.parse(await fs.readFile(path.join(cancelOut, 'session.json'), 'utf8'));
  assert.equal(await isListening(cancelSession.port), false, 'cancelled server leaked');
  const remaining = (await processRows()).filter(
    (row) => row.pgid === ownedGroup && !row.state.startsWith('Z')
  );
  assert.equal(remaining.length, 0, 'owned Expo/worker process group remained live');
  assert.ok(ownedMembers.length > 0);
  assert.equal(await fs.stat(path.join(cancelOut, 'result.json')).catch(() => null), null);
  console.log(
    'Expo probe lifecycle: spawn/setup errors, rejected reload protocol and cancellation restore source, stop owned servers and emit no success record.'
  );
} finally {
  for (const state of processes) {
    if (state.child.exitCode === null && state.child.signalCode === null)
      state.child.kill('SIGTERM');
    await finish(state);
  }
  await fs.rm(temporary, { recursive: true, force: true });
}
