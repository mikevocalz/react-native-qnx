#!/usr/bin/env node
// Host-only Expo CLI integration probe. No device is contacted or launched.
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { createHash } from 'node:crypto';
import { mkdir, readFile, writeFile, appendFile, rename, rm, realpath } from 'node:fs/promises';
import { createRequire } from 'node:module';
import net from 'node:net';
import path from 'node:path';
import { parseArgs } from 'node:util';
import { setTimeout as delay } from 'node:timers/promises';

const { values } = parseArgs({
  options: {
    out: { type: 'string' },
    python: { type: 'string', default: 'python3' },
    project: { type: 'string' },
    source: { type: 'string' },
    text: { type: 'string' },
  },
  allowPositionals: false,
});
if (!values.out) throw new Error('Use --out <NEW evidence directory> (prefer an external volume).');
if (process.platform === 'win32') throw new Error('This POSIX host probe requires macOS or Linux.');
const out = path.resolve(values.out);
await mkdir(out, { recursive: false });
const root = path.resolve(import.meta.dirname, '..');
const project = await realpath(
  path.resolve(values.project ?? path.join(root, 'apps/expo-router-example'))
);
const projectRequire = createRequire(path.join(project, 'package.json'));
const expo = path.dirname(projectRequire.resolve('expo/package.json'));
const cli = path.join(expo, 'bin/cli');
const relativeSource = values.source ?? 'app/index.js';
if (
  path.isAbsolute(relativeSource) ||
  relativeSource.split(/[\\/]/).some((part) => !part || part === '.' || part === '..')
)
  throw new Error('Use a confined project-relative source file');
const sourcePath = await realpath(path.join(project, relativeSource));
if (!sourcePath.startsWith(project + path.sep))
  throw new Error('Probe source must stay inside its project');
const probeText = values.text ?? 'Your draft';
if (!probeText || probeText.length > 2000)
  throw new Error('Probe text must be nonempty and bounded');
const original = await readFile(sourcePath, 'utf8');
const marker = 'RNQNX_EXPO_HOST_WATCH_CONTROL';
assert.ok(original.includes(probeText) && !original.includes(marker));
const edited = original.replace(probeText, `${probeText} ${marker}`);
const server = net.createServer();
await new Promise((resolve) => server.listen(0, '127.0.0.1', () => resolve(undefined)));
const address = server.address();
if (!address || typeof address === 'string') throw new Error('No localhost port allocated.');
const port = address.port;
await new Promise((resolve, reject) =>
  server.close((error) => (error ? reject(error) : resolve(undefined)))
);
/** @type {NodeJS.ProcessEnv} */
const env = { ...process.env, EXPO_NO_TELEMETRY: '1' };
// Expo CLI documents that CI=1 disables its file watcher. This probe specifically
// tests file watching; only the child server receives the adjusted environment.
delete env.CI;
const log = path.join(out, 'expo-start.log');
/** @param {string} text */
const quote = (text) => "'" + text.replaceAll("'", "'\"'\"'") + "'";
const wrapper = path.join(out, 'run-expo.sh');
await writeFile(
  wrapper,
  '#!/bin/sh\nexec ' +
    [
      process.execPath,
      cli,
      'start',
      '--offline',
      '--dev-client',
      '--max-workers',
      '2',
      '--port',
      String(port),
    ]
      .map(quote)
      .join(' ') +
    '\n',
  { mode: 0o755, flag: 'wx' }
);
// Prepare metadata before launching. All fallible post-spawn setup is inside
// the protected try/finally below. The supervisor reserves the actual Expo
// leader PID while terminating its complete owned process group.
await writeFile(path.join(out, 'session.json'), JSON.stringify({ port, supervised: true }) + '\n');
const child = spawn(
  values.python,
  [path.join(root, 'scripts/qt-public/guarded-run.py'), wrapper, log],
  {
    cwd: project,
    env: { ...env, MIN_KB: '0', GUARD_PATH: project, GUARD_INTERVAL_S: '0.25', GUARD_GRACE_S: '1' },
    stdio: ['ignore', 'pipe', 'pipe'],
  }
);
/** @type {Error | undefined} */
let spawnError;
child.once('error', (error) => {
  spawnError = error;
});
/** @type {Promise<void>[]} */
const pendingLogs = [];
for (const stream of [child.stdout, child.stderr])
  stream.on('data', (data) => {
    pendingLogs.push(appendFile(log, data));
  });
const closed = new Promise((resolve) => child.once('close', resolve));
const controller = new AbortController();
const stop = () => {
  controller.abort(new Error('Expo host probe cancelled'));
  child.kill('SIGINT');
};
process.once('SIGINT', stop);
process.once('SIGTERM', stop);
const base = `http://127.0.0.1:${port}`;
const url = `${base}/index.bundle?platform=qnx&dev=true&minify=false`;
/** @param {() => Promise<boolean>} predicate @param {number} ms */
async function until(predicate, ms) {
  const deadline = Date.now() + ms;
  while (Date.now() < deadline) {
    controller.signal.throwIfAborted();
    if (spawnError) throw spawnError;
    if (child.exitCode !== null || child.signalCode !== null)
      throw new Error(`Expo exited ${child.exitCode}`);
    try {
      if (await predicate()) return;
    } catch (error) {
      if (error instanceof Error && !/fetch failed/.test(error.message)) throw error;
    }
    await delay(250);
  }
  throw new Error('Timed out waiting for Expo host observation.');
}
/** @param {string} name */
async function bundle(name) {
  const response = await fetch(url, {
    signal: AbortSignal.any([controller.signal, AbortSignal.timeout(120_000)]),
  });
  const body = await response.text();
  await writeFile(path.join(out, name), body);
  assert.equal(response.status, 200, body.slice(0, 1000));
  return body;
}
const temporarySource = `${sourcePath}.rnqnx-probe-${process.pid}`;
let temporaryOwned = false;
/** @param {string} expected @param {string} replacement */
async function replaceSource(expected, replacement) {
  await writeFile(temporarySource, replacement, { flag: 'wx' });
  temporaryOwned = true;
  assert.equal(await readFile(sourcePath, 'utf8'), expected, 'Concurrent source edit detected');
  await rename(temporarySource, sourcePath);
  temporaryOwned = false;
}
/** @type {Record<string, unknown> | undefined} */
let result;
/** @type {WebSocket | undefined} */
let reloadSocket;
let changed = false;
try {
  await writeFile(
    path.join(out, 'session.json'),
    JSON.stringify({ port, supervisorPid: child.pid }) + '\n'
  );
  await until(
    async () =>
      (
        await fetch(`${base}/status`, {
          signal: AbortSignal.any([controller.signal, AbortSignal.timeout(5000)]),
        })
      ).ok,
    30_000
  );
  const initial = await bundle('initial.bundle.js');
  assert.ok(initial.includes(probeText) && !initial.includes(marker));
  await replaceSource(original, edited);
  changed = true;
  let watched = '';
  await until(async () => {
    watched = await bundle('watched.bundle.js');
    return watched.includes(marker);
  }, 30_000);
  // Refuse to overwrite a concurrent edit while cleaning up the probe change.
  assert.equal(await readFile(sourcePath, 'utf8'), edited);
  await replaceSource(edited, original);
  changed = false;
  let restored = '';
  await until(async () => {
    restored = await bundle('restored.bundle.js');
    return !restored.includes(marker);
  }, 30_000);
  // Observe the real SDK message endpoint with a synthetic host listener. This
  // proves broadcast delivery through Expo CLI, not native client execution.
  reloadSocket = new WebSocket(`ws://127.0.0.1:${port}/message`);
  let reloadObserved = false;
  let socketFailed = false;
  reloadSocket.addEventListener('error', () => {
    socketFailed = true;
  });
  reloadSocket.addEventListener('message', (event) => {
    try {
      if (JSON.parse(String(event.data)).method === 'reload') reloadObserved = true;
    } catch {
      /* Other SDK socket payloads are not reload observations. */
    }
  });
  await until(async () => {
    if (socketFailed) throw new Error('Host reload observer failed to connect');
    return reloadSocket?.readyState === WebSocket.OPEN;
  }, 5000);
  // SDK58 reload is a v2 message-socket command. An arbitrary HTTP 200
  // from /reload is insufficient: Expo may answer it through fallback routing.
  reloadSocket.send(JSON.stringify({ version: 2, method: 'reload' }));
  await until(async () => reloadObserved, 5000);
  const initialSha256 = createHash('sha256').update(initial).digest('hex');
  const restoredSha256 = createHash('sha256').update(restored).digest('hex');
  assert.equal(restoredSha256, initialSha256, 'Restored bundle must exactly match the original');
  assert.equal(
    await readFile(sourcePath, 'utf8'),
    original,
    'Source changed before result recording'
  );
  result = {
    scope: 'host-only Expo CLI Metro source watching; no device or native Fast Refresh acceptance',
    node: process.version,
    project,
    source: path.relative(project, sourcePath),
    port,
    initialBytes: Buffer.byteLength(initial),
    watchedBytes: Buffer.byteLength(watched),
    restoredBytes: Buffer.byteLength(restored),
    reloadTransport: 'Expo SDK v2 message socket',
    reloadProtocolVersion: 2,
    reloadBroadcastObserved: reloadObserved,
    reloadObserver: 'synthetic host WebSocket; no native client delivery claim',
    initialSha256,
    restoredSha256,
    sourceRestored: (await readFile(sourcePath, 'utf8')) === original,
  };
} finally {
  try {
    if (changed) {
      if ((await readFile(sourcePath, 'utf8')) === edited) await replaceSource(edited, original);
      else console.error('Concurrent source edit detected; probe did not overwrite it.');
    }
  } finally {
    reloadSocket?.close();
    child.kill('SIGINT');
    let stopped = await Promise.race([
      closed.then(() => true),
      delay(5000, undefined, { ref: false }).then(() => false),
    ]);
    if (!stopped) {
      child.kill('SIGTERM');
      stopped = await Promise.race([
        closed.then(() => true),
        delay(5000, undefined, { ref: false }).then(() => false),
      ]);
    }
    if (!stopped)
      throw new Error(
        `Owned Expo supervisor ${child.pid} did not stop within the cleanup deadline`
      );
    const listening = await new Promise((resolve) => {
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
    assert.equal(
      listening,
      false,
      'Expo port remained open after supervisor process-group cleanup'
    );
    await Promise.all(pendingLogs);
    if (temporaryOwned) await rm(temporarySource, { force: true });
    process.removeListener('SIGINT', stop);
    process.removeListener('SIGTERM', stop);
  }
}

assert.ok(result);
result.serverStopped = true;
await writeFile(path.join(out, 'result.json'), JSON.stringify(result, null, 2) + '\n');
console.log(JSON.stringify(result, null, 2));
