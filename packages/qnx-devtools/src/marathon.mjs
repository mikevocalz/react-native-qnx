#!/usr/bin/env node
import { spawn } from 'node:child_process';
import { constants } from 'node:fs';
import { access, lstat, open, realpath } from 'node:fs/promises';
import path from 'node:path';
import { parseArgs } from 'node:util';
import { fileURLToPath } from 'node:url';

const help = `React Native QNX: local Marathon development client
  inspect --runner <native-ELF> --bundle <file>
  run --runner <native-ELF> --bundle <file> [--timeout <1-3600>] [--dry-run]

Run this on the Linux target, in an existing Wayland session. Timeout defaults
to 300 seconds. This launches a trusted executable, not a sandboxed .marathon
package, a remote deployment, or proof of actual Marathon shell integration.
The existing runner's app identity and registered surface are unchanged.
`;

/** @param {string} input @param {number} limit */
async function regularFile(input, limit) {
  const file = path.resolve(input);
  const info = await lstat(file);
  if (!info.isFile() || info.isSymbolicLink() || info.size < 1 || info.size > limit) {
    throw new Error('Expected a nonempty regular file within the size limit.');
  }
  if ((await realpath(file)) !== file) throw new Error('Symlink paths are not accepted.');
  return { file, size: info.size };
}

/** Validate a dynamic Linux ELF header without invoking ldd or executing it.
 * @param {string} input */
export async function inspectRunner(input) {
  if (process.platform !== 'linux') throw new Error('The development launcher requires Linux.');
  const { file, size } = await regularFile(input, 512 * 1024 * 1024);
  await access(file, constants.X_OK);
  const handle = await open(file, 'r');
  try {
    const header = Buffer.alloc(64);
    if (
      (await handle.read(header, 0, 64, 0)).bytesRead !== 64 ||
      !header.subarray(0, 4).equals(Buffer.from([127, 69, 76, 70])) ||
      header[4] !== 2 ||
      header[5] !== 1 ||
      header[6] !== 1 ||
      ![2, 3].includes(header.readUInt16LE(16))
    ) {
      throw new Error('Runner must be a 64-bit little-endian executable ELF.');
    }
    const machine = header.readUInt16LE(18);
    const arch = machine === 62 ? 'x64' : machine === 183 ? 'arm64' : 'unsupported';
    if (arch !== process.arch) throw new Error('Runner architecture does not match this host.');
    const start = Number(header.readBigUInt64LE(32));
    const stride = header.readUInt16LE(54);
    const count = header.readUInt16LE(56);
    if (
      !Number.isSafeInteger(start) ||
      start < 64 ||
      start > 65536 ||
      stride !== 56 ||
      count < 1 ||
      count > 128 ||
      start + stride * count > size
    ) {
      throw new Error('Invalid ELF program-header bounds.');
    }
    const headers = Buffer.alloc(stride * count);
    if ((await handle.read(headers, 0, headers.length, start)).bytesRead !== headers.length) {
      throw new Error('Truncated ELF program headers.');
    }
    let interpreter = '';
    for (let index = 0; index < count; index += 1) {
      const row = headers.subarray(index * stride, (index + 1) * stride);
      if (row.readUInt32LE(0) !== 3) continue;
      if (interpreter) throw new Error('Duplicate ELF interpreter.');
      const offset = Number(row.readBigUInt64LE(8));
      const length = Number(row.readBigUInt64LE(32));
      if (
        !Number.isSafeInteger(offset) ||
        offset < 0 ||
        length < 2 ||
        length > 4096 ||
        offset + length > size
      )
        throw new Error('Invalid ELF interpreter bounds.');
      const bytes = Buffer.alloc(length);
      if (
        (await handle.read(bytes, 0, length, offset)).bytesRead !== length ||
        bytes[length - 1] !== 0 ||
        bytes.subarray(0, -1).includes(0)
      ) {
        throw new Error('Invalid ELF interpreter string.');
      }
      interpreter = bytes.subarray(0, -1).toString('utf8');
    }
    const loaders =
      arch === 'x64'
        ? ['/lib64/ld-linux-x86-64.so.2', '/lib/ld-musl-x86_64.so.1']
        : ['/lib/ld-linux-aarch64.so.1', '/lib/ld-musl-aarch64.so.1'];
    if (!loaders.includes(interpreter)) throw new Error('Unsupported or missing Linux ELF loader.');
    await access(interpreter, constants.X_OK);
    return { file, arch, interpreter, sharedLibrariesVerified: false };
  } finally {
    await handle.close();
  }
}

/** @param {NodeJS.ProcessEnv} env */
export async function inspectWayland(env) {
  const runtime = env.XDG_RUNTIME_DIR;
  const display = env.WAYLAND_DISPLAY;
  if (
    !runtime ||
    !path.isAbsolute(runtime) ||
    !display ||
    !/^[a-zA-Z0-9][a-zA-Z0-9._-]{0,127}$/.test(display) ||
    display === '..'
  ) {
    throw new Error('Set an absolute XDG_RUNTIME_DIR and a simple WAYLAND_DISPLAY socket name.');
  }
  const info = await lstat(runtime);
  if (
    !info.isDirectory() ||
    info.isSymbolicLink() ||
    (await realpath(runtime)) !== runtime ||
    info.uid !== process.getuid?.() ||
    (info.mode & 0o077) !== 0
  ) {
    throw new Error('Wayland runtime directory must be a private directory owned by this user.');
  }
  const socket = path.join(runtime, display);
  const endpoint = await lstat(socket);
  if (!endpoint.isSocket() || endpoint.uid !== process.getuid?.()) {
    throw new Error('Wayland endpoint must be a socket owned by this user.');
  }
  return { socket, compositorVerified: false };
}

/** @param {string} runner @param {string} bundle @param {number} timeoutSeconds */
async function launch(runner, bundle, timeoutSeconds) {
  /** @type {NodeJS.ProcessEnv} */
  const env = { ...process.env, QT_QPA_PLATFORM: 'wayland' };
  delete env.WAYLAND_SOCKET; // A stale inherited descriptor would bypass the checked socket.
  return new Promise((/** @type {(value: number) => void} */ resolve, reject) => {
    const child = spawn(runner, ['--bundle', bundle], {
      cwd: path.dirname(bundle),
      env,
      shell: false,
      detached: true,
      stdio: 'inherit',
    });
    let reason = 0;
    /** @type {ReturnType<typeof setTimeout> | undefined} */
    let escalation;
    /** @param {NodeJS.Signals} signal */
    const signalGroup = (signal) => {
      if (!child.pid) return;
      try {
        process.kill(-child.pid, signal);
      } catch (error) {
        if (!(error instanceof Error && 'code' in error && error.code === 'ESRCH')) {
          // Record cleanup failure without throwing from an asynchronous handler.
          reason = reason || 1;
          console.error('rnqnx: could not signal the owned development process group.');
        }
      }
    };
    /** @param {number} code @param {NodeJS.Signals} signal */
    const stop = (code, signal) => {
      if (reason) return;
      reason = code;
      signalGroup(signal);
      escalation = setTimeout(() => signalGroup('SIGKILL'), 250);
    };
    const interrupt = () => stop(130, 'SIGINT');
    const terminate = () => stop(143, 'SIGTERM');
    const timer = setTimeout(() => stop(124, 'SIGTERM'), timeoutSeconds * 1000);
    const cleanup = () => {
      clearTimeout(timer);
      if (escalation) clearTimeout(escalation);
      process.off('SIGINT', interrupt);
      process.off('SIGTERM', terminate);
    };
    process.on('SIGINT', interrupt);
    process.on('SIGTERM', terminate);
    child.once('error', (error) => {
      cleanup();
      reject(error);
    });
    child.once('close', (code, signal) => {
      signalGroup('SIGKILL');
      cleanup();
      resolve(reason || (code ?? (signal === 'SIGINT' ? 130 : signal === 'SIGTERM' ? 143 : 1)));
    });
  });
}

/** @param {string[]} argv */
export async function marathonMain(argv) {
  const { values, positionals, tokens } = parseArgs({
    args: argv,
    strict: true,
    allowPositionals: true,
    tokens: true,
    options: {
      help: { type: 'boolean', short: 'h' },
      runner: { type: 'string' },
      bundle: { type: 'string' },
      timeout: { type: 'string' },
      'dry-run': { type: 'boolean' },
    },
  });
  if (values.help || argv.length === 0) {
    console.log(help);
    return 0;
  }
  const seen = new Set();
  for (const token of tokens) {
    if (token.kind !== 'option') continue;
    if (seen.has(token.name)) throw new Error(`Duplicate option --${token.name}.`);
    seen.add(token.name);
  }
  const [command] = positionals;
  if (positionals.length !== 1 || !['inspect', 'run'].includes(command)) {
    throw new Error('Use marathon inspect or run.');
  }
  if (!values.runner?.trim() || !values.bundle?.trim())
    throw new Error('--runner and --bundle are required.');
  if (command === 'inspect' && (values.timeout !== undefined || values['dry-run'])) {
    throw new Error('inspect accepts only --runner and --bundle.');
  }
  const timeout = values.timeout ?? '300';
  if (!/^[1-9][0-9]{0,3}$/.test(timeout) || Number(timeout) > 3600) {
    throw new Error('--timeout must be an integer from 1 to 3600 seconds.');
  }
  const runner = await inspectRunner(values.runner);
  const bundle = await regularFile(values.bundle, 128 * 1024 * 1024);
  const report = {
    runner,
    bundle: bundle.file,
    appIdentityVerified: false,
    managedPackage: false,
    runtimeVerified: false,
  };
  if (command === 'inspect') {
    console.log(JSON.stringify(report, null, 2));
    return 0;
  }
  const session = await inspectWayland(process.env);
  if (values['dry-run']) {
    console.log(
      JSON.stringify(
        {
          ...report,
          session,
          executable: runner.file,
          args: ['--bundle', bundle.file],
          timeoutSeconds: Number(timeout),
          environmentOverrides: { QT_QPA_PLATFORM: 'wayland', WAYLAND_SOCKET: null },
        },
        null,
        2
      )
    );
    return 0;
  }
  return launch(runner.file, bundle.file, Number(timeout));
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    process.exitCode = await marathonMain(process.argv.slice(2));
  } catch (error) {
    console.error(`rnqnx marathon: ${error instanceof Error ? error.message : error}`);
    process.exitCode = 1;
  }
}
