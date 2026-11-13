import { spawn } from 'node:child_process';
import dgram from 'node:dgram';
import { lstat, readFile, realpath } from 'node:fs/promises';
import { createRequire } from 'node:module';
import os from 'node:os';
import path from 'node:path';
import { parseArgs } from 'node:util';

// `project start` runs Metro for a QNX app with the endpoints the development
// client uses: /<entry>.bundle, /hot (Fast Refresh) and /message (reload and
// developer menu from this terminal). `project dev` also launches the
// development client on a board over SSH, pointed at this Metro.

const startHelp = `React Native QNX - Metro for the development client
  project start [--project <dir>] [--platform qnx|marathon] [--port 8081]
                [--host 0.0.0.0] [--reset-cache]

Serves the app's bundle for --platform with Fast Refresh. Keys: r reload the
app, d open its developer menu, Ctrl+C quit. Point a development build at it
with --dev-server http://<this computer>:<port>.
`;

const devHelp = `React Native QNX - start Metro and launch the development client on a board
  project dev --device <user@host> --launcher <remote script>
              [--project <dir>] [--platform qnx|marathon] [--port 8081]
              [--identity <ssh key>] [--known-hosts <file>]
              [--host-address <this computer's address as the board sees it>]
              [--reset-cache]

Starts Metro (as project start does), then runs
  <launcher> --dev-server http://<host-address>:<port>
on the board over SSH. The launcher is the script that starts a build made
with RNQNX_ENABLE_DEV_SUPPORT=ON and passes its arguments through. SSH uses
BatchMode and your known_hosts (or --known-hosts); host keys are never skipped.
`;

/** The part of a `ws` socket this file uses (ws comes from Metro's graph).
 * @typedef {{readyState: number, send(data: string): void, terminate(): void,
 *   on(event: string, listener: (...args: any[]) => void): void}} MessageClient */

/** @param {unknown} error */
function missing(error) {
  return error instanceof Error && 'code' in error && error.code === 'ENOENT';
}

/** The config `project bundle` would use for this platform.
 * @param {string} root @param {string} platform */
async function metroConfigPath(root, platform) {
  const names =
    platform === 'qnx' ? ['metro.qnx.config.js', 'metro.config.js'] : ['metro.marathon.config.js'];
  for (const name of names) {
    const file = path.join(root, name);
    try {
      const stat = await lstat(file);
      if (stat.isFile()) return file;
    } catch (error) {
      if (!missing(error)) throw error;
    }
  }
  throw new Error(`No ${names.join(' or ')} in ${root}.`);
}

/** @param {string | undefined} text */
function parsePort(text) {
  const port = Number(text ?? 8081);
  if (!Number.isInteger(port) || port < 1 || port > 65535)
    throw new Error('--port must be 1-65535.');
  return port;
}

/** IPv4 addresses a board on a local link could reach. */
export function localAddresses() {
  const found = [];
  for (const [name, list] of Object.entries(os.networkInterfaces()))
    for (const entry of list ?? [])
      if (entry.family === 'IPv4' && !entry.internal) found.push({ name, address: entry.address });
  return found;
}

/** The local address the OS would use to reach `host` (no packet is sent).
 * @param {string} host */
export function addressTowards(host) {
  return new Promise((resolve, reject) => {
    const socket = dgram.createSocket(host.includes(':') ? 'udp6' : 'udp4');
    socket.on('error', (error) => {
      socket.close();
      reject(error);
    });
    socket.connect(9, host, () => {
      const { address } = socket.address();
      socket.close();
      resolve(address);
    });
  });
}

/** The RN community message protocol: {version: 2, method, params}.
 * @param {Set<MessageClient>} clients @param {string} method */
function broadcast(clients, method) {
  const message = JSON.stringify({ version: 2, method, params: null });
  let sent = 0;
  for (const client of clients) {
    if (client.readyState === 1) {
      client.send(message);
      sent += 1;
    }
  }
  return sent;
}

/**
 * Runs Metro for `root` and resolves once it is listening.
 * @param {{project: string, platform?: string, port?: number, host?: string, resetCache?: boolean, interactive?: boolean, log?: (line: string) => void}} options
 */
export async function startDevServer(options) {
  const platform = options.platform ?? 'qnx';
  if (!['qnx', 'marathon'].includes(platform)) throw new Error('Use --platform qnx or marathon.');
  const log = options.log ?? ((line) => console.log(line));
  const root = await realpath(path.resolve(options.project));
  const manifest = path.join(root, 'package.json');
  JSON.parse(await readFile(manifest, 'utf8'));
  const config = await metroConfigPath(root, platform);
  const require = createRequire(manifest);
  let Metro;
  try {
    Metro = require('metro');
  } catch {
    throw new Error(`Metro is not installed for ${root}. Install the app's dependencies first.`);
  }
  // `ws` comes from Metro's own dependency graph, so this adds no package.
  const metroRequire = createRequire(require.resolve('metro/package.json'));
  const ws = metroRequire('ws');
  // ws 7 (Metro 0.87) exports Server; ws 8 also names it WebSocketServer.
  const WebSocketServer = ws.WebSocketServer ?? ws.Server;

  const port = options.port ?? 8081;
  const host = options.host ?? '0.0.0.0';
  const loaded = await Metro.loadConfig({
    cwd: root,
    config,
    port,
    resetCache: !!options.resetCache,
  });
  // Assets outside the project root (React Native's LogBox images in a
  // monorepo or pnpm store) get an httpServerLocation with `../`, which
  // Metro's URL parsing removes, so they 404. `unstable_path` carries the
  // relative path in the query instead (the same scheme Expo CLI uses). Only
  // the bundles this server builds change; release bundles keep their paths.
  const metroConfig = {
    ...loaded,
    transformer: { ...loaded.transformer, publicPath: '/assets?unstable_path=.' },
  };

  /** @type {Set<MessageClient>} */
  const clients = new Set();
  const messageServer = new WebSocketServer({ noServer: true });
  messageServer.on('connection', (/** @type {MessageClient} */ socket) => {
    clients.add(socket);
    log(`development client connected (${clients.size} on /message)`);
    socket.on('close', () => clients.delete(socket));
    socket.on('error', () => clients.delete(socket));
    // Clients only listen on this socket; nothing they send is acted on.
  });

  /** @param {import('http').IncomingMessage} req @param {import('http').ServerResponse} res @param {() => void} next */
  const status = (req, res, next) => {
    if (req.url === '/status') {
      res.setHeader('Content-Type', 'text/plain');
      res.end('packager-status:running');
      return;
    }
    next();
  };

  // Metro 0.87 resolves to {httpServer}; older lines returned the server.
  const started = await Metro.runServer(metroConfig, {
    host,
    unstable_extraMiddleware: [status],
    websocketEndpoints: { '/message': messageServer },
  });
  const server = started.httpServer ?? started;

  const shown = host === '0.0.0.0' ? localAddresses().map((a) => a.address) : [host];
  log(`Metro for ${platform} on port ${port} (${path.basename(root)})`);
  for (const address of shown) log(`  --dev-server http://${address}:${port}`);

  const reload = () => {
    const sent = broadcast(clients, 'reload');
    log(sent ? `reload sent to ${sent} client(s)` : 'no development client connected');
  };
  const devMenu = () => {
    const sent = broadcast(clients, 'devMenu');
    log(sent ? `developer menu opened on ${sent} client(s)` : 'no development client connected');
  };

  let detachKeys = () => {};
  if (options.interactive && process.stdin.isTTY) {
    log('Keys: r reload · d developer menu · Ctrl+C quit');
    process.stdin.setRawMode(true);
    process.stdin.resume();
    process.stdin.setEncoding('utf8');
    /** @param {string} key */
    const onKey = (key) => {
      if (key === '\u0003') process.kill(process.pid, 'SIGINT');
      else if (key === 'r') reload();
      else if (key === 'd') devMenu();
    };
    process.stdin.on('data', onKey);
    detachKeys = () => {
      process.stdin.off('data', onKey);
      if (process.stdin.isTTY) process.stdin.setRawMode(false);
      process.stdin.pause();
    };
  }

  const close = () =>
    new Promise((resolve) => {
      detachKeys();
      for (const client of clients) client.terminate();
      messageServer.close();
      server.close(() => resolve(undefined));
      // Keep-alive bundle requests and the /hot socket would hold close() open.
      server.closeAllConnections?.();
    });

  return { port, host, root, reload, devMenu, close, clients };
}

/** Resolves when the process gets SIGINT or SIGTERM. */
function untilInterrupted() {
  return new Promise((resolve) => {
    process.once('SIGINT', resolve);
    process.once('SIGTERM', resolve);
  });
}

/** Stops Metro and ends the process: Metro's file watchers would otherwise
 * keep Node alive after the server closes.
 * @param {{close: () => Promise<unknown>}} server */
async function shutdown(server) {
  await Promise.race([server.close(), new Promise((resolve) => setTimeout(resolve, 2000))]);
  process.exit(0);
}

/** @param {string[]} argv */
export async function startMain(argv) {
  const { values, positionals } = parseArgs({
    args: argv,
    strict: true,
    allowPositionals: true,
    options: {
      help: { type: 'boolean', short: 'h' },
      project: { type: 'string' },
      platform: { type: 'string' },
      port: { type: 'string' },
      host: { type: 'string' },
      'reset-cache': { type: 'boolean' },
    },
  });
  if (values.help) {
    console.log(startHelp);
    return 0;
  }
  if (positionals.length) throw new Error('project start accepts no positional arguments.');
  const server = await startDevServer({
    project: values.project ?? '.',
    platform: values.platform,
    port: parsePort(values.port),
    host: values.host,
    resetCache: values['reset-cache'],
    interactive: true,
  });
  await untilInterrupted();
  await shutdown(server);
  return 0;
}

/**
 * The ssh arguments for launching the development client.
 * @param {{device: string, launcher: string, url: string, identity?: string, knownHosts?: string}} options
 */
export function launchCommand(options) {
  // No leading '-': ssh would read the value as an option.
  if (!/^[A-Za-z0-9_][A-Za-z0-9._-]*@[A-Za-z0-9_[][A-Za-z0-9.:_[\]-]*$/.test(options.device))
    throw new Error('--device must be user@host.');
  if (!/^\/[A-Za-z0-9._/-]+$/.test(options.launcher))
    throw new Error('--launcher must be an absolute path on the board (letters, digits, . _ - /).');
  if (!/^https?:\/\/[A-Za-z0-9.:[\]-]+:\d+$/.test(options.url))
    throw new Error(`Unexpected dev server URL ${options.url}.`);
  const args = ['-o', 'BatchMode=yes', '-o', 'ConnectTimeout=10'];
  if (options.identity) args.push('-i', options.identity);
  if (options.knownHosts) args.push('-o', `UserKnownHostsFile=${options.knownHosts}`);
  // Detach on the board so the app outlives this SSH session; its own
  // launcher decides where output goes.
  const remote = `nohup ${options.launcher} --dev-server ${options.url} </dev/null >/dev/null 2>&1 &`;
  args.push('--', options.device, remote);
  return args;
}

/** @param {string} command @param {string[]} args */
function run(command, args) {
  return new Promise((resolve, reject) => {
    const child = spawn(command, args, { stdio: ['ignore', 'inherit', 'inherit'] });
    child.on('error', reject);
    child.on('exit', (code) => resolve(code ?? 1));
  });
}

/** @param {string[]} argv */
export async function devMain(argv) {
  const { values, positionals } = parseArgs({
    args: argv,
    strict: true,
    allowPositionals: true,
    options: {
      help: { type: 'boolean', short: 'h' },
      project: { type: 'string' },
      platform: { type: 'string' },
      port: { type: 'string' },
      device: { type: 'string' },
      launcher: { type: 'string' },
      identity: { type: 'string' },
      'known-hosts': { type: 'string' },
      'host-address': { type: 'string' },
      'reset-cache': { type: 'boolean' },
    },
  });
  if (values.help) {
    console.log(devHelp);
    return 0;
  }
  if (positionals.length) throw new Error('project dev accepts no positional arguments.');
  if (!values.device || !values.launcher)
    throw new Error('project dev needs --device and --launcher.');
  const port = parsePort(values.port);
  const boardHost = values.device.split('@')[1].replace(/^\[|\]$/g, '');
  const hostAddress = values['host-address'] ?? (await addressTowards(boardHost));
  const url = `http://${hostAddress}:${port}`;
  const sshArgs = launchCommand({
    device: values.device,
    launcher: values.launcher,
    url,
    identity: values.identity,
    knownHosts: values['known-hosts'],
  });

  const server = await startDevServer({
    project: values.project ?? '.',
    platform: values.platform,
    port,
    resetCache: values['reset-cache'],
    interactive: true,
  });
  console.log(`Launching ${values.launcher} on ${values.device} with --dev-server ${url}`);
  const code = await run('ssh', sshArgs);
  if (code !== 0) {
    console.error(
      `ssh exited with ${code}; Metro keeps running. Launch the app by hand with --dev-server ${url}.`
    );
  } else {
    console.log('Launched. The app loads the bundle, then connects for Fast Refresh.');
  }
  await untilInterrupted();
  await shutdown(server);
  return 0;
}
