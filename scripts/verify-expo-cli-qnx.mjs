#!/usr/bin/env node
// End-to-end check for the `expo` gate: create a clean project with `rnqnx project create`,
// install it, start it with Expo CLI (`expo start --offline`) and request the QNX bundle.
// Exits 0 only when Expo CLI answers HTTP 200 for platform=qnx. Host only; needs the network
// for `pnpm install`. Usage: node scripts/verify-expo-cli-qnx.mjs [--keep] [--log-dir <dir>]
// Set RNQNX_PNPM (for example "npx -y pnpm@12.21.0") when `pnpm` on PATH is not the pinned one.
import { spawn, spawnSync } from 'node:child_process';
import { mkdir, mkdtemp, rm, writeFile } from 'node:fs/promises';
import net from 'node:net';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { parseArgs } from 'node:util';

const { values } = parseArgs({
  options: { keep: { type: 'boolean' }, 'log-dir': { type: 'string' } },
});
const repo = path.resolve(import.meta.dirname, '..');
const work = await mkdtemp(path.join(tmpdir(), 'rnqnx-expo-cli-'));
const project = path.join(work, 'notes');
const logDir = values['log-dir'] ? path.resolve(values['log-dir']) : undefined;
if (logDir) await mkdir(logDir, { recursive: true });
const pnpm = (process.env.RNQNX_PNPM || 'pnpm').split(' ');

/**
 * @param {string} name
 * @param {string} command
 * @param {string[]} args
 * @param {import('node:child_process').SpawnSyncOptions} [options]
 */
function step(name, command, args, options = {}) {
  const result = spawnSync(command, args, { encoding: 'utf8', ...options });
  const text = `$ ${[command, ...args].join(' ')}\n${result.stdout ?? ''}${result.stderr ?? ''}`;
  if (logDir) return writeFile(path.join(logDir, `${name}.log`), text).then(() => result);
  return Promise.resolve(result);
}

async function freePort() {
  return new Promise((resolve, reject) => {
    const server = net.createServer();
    server.listen(0, '127.0.0.1', () => {
      const { port } = /** @type {net.AddressInfo} */ (server.address());
      server.close(() => resolve(port));
    });
    server.on('error', reject);
  });
}

let server;
let failed = false;
try {
  let r = await step('create', process.execPath, [
    path.join(repo, 'packages', 'react-native-qnx-cli', 'src', 'bin.mjs'),
    'project',
    'create',
    '--project',
    project,
    '--app-id',
    'org.example.notes',
    '--name',
    'Notes',
  ]);
  if (r.status !== 0) throw new Error('rnqnx project create failed');
  r = await step('install', pnpm[0], [...pnpm.slice(1), 'install', '--ignore-scripts'], {
    cwd: project,
  });
  if (r.status !== 0) throw new Error('pnpm install failed');

  const port = await freePort();
  server = spawn(
    pnpm[0],
    [...pnpm.slice(1), 'exec', 'expo', 'start', '--offline', '--port', String(port)],
    { cwd: project, env: { ...process.env, CI: '1', EXPO_NO_TELEMETRY: '1' } }
  );
  let serverLog = '';
  server.stdout.on('data', (d) => (serverLog += d));
  server.stderr.on('data', (d) => (serverLog += d));
  for (let i = 0; i < 120 && !serverLog.includes('Waiting on'); i += 1) {
    await new Promise((resolve) => setTimeout(resolve, 500));
  }
  if (!serverLog.includes('Waiting on')) throw new Error('expo start did not become ready');

  for (const query of ['dev=true', 'dev=false&minify=false']) {
    const url = `http://127.0.0.1:${port}/index.bundle?platform=qnx&${query}`;
    const response = await fetch(url);
    const body = Buffer.from(await response.arrayBuffer());
    const line = `GET ${url.replace(String(port), '<port>')} -> HTTP ${response.status}, ${body.length} bytes`;
    console.log(line);
    if (logDir)
      await writeFile(path.join(logDir, `bundle-${query.split('&')[0]}.txt`), line + '\n');
    if (
      response.status !== 200 ||
      !body.subarray(0, 64).toString().includes('__BUNDLE_START_TIME__')
    ) {
      console.error(body.subarray(0, 400).toString());
      failed = true;
    }
  }
  if (logDir) await writeFile(path.join(logDir, 'expo-start.log'), serverLog);
} catch (error) {
  console.error(error instanceof Error ? error.message : error);
  failed = true;
} finally {
  server?.kill();
  if (!values.keep) await rm(work, { recursive: true, force: true });
  else console.log(`kept ${work}`);
}
process.exitCode = failed ? 1 : 0;
