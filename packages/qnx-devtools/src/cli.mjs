#!/usr/bin/env node
import { spawn } from 'node:child_process';
import { randomBytes } from 'node:crypto';
import { lstat, readFile, stat, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { parseArgs } from 'node:util';
import { fileURLToPath } from 'node:url';

const MAX_CAPTURE_BYTES = 32 * 1024 * 1024;
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
const help = `QNX device tools (source checkout)

Usage: pnpm qnx:device <command> [options]

  status                         Read OS/architecture and tool availability
  logs [--follow]                Read slog2 once, or stream until Ctrl-C
  screenshot --out <new.bmp>     Capture the display into a NEW local file
  port-forward --local-port <n> --remote-port <n>
                                 Forward localhost to target localhost

All commands: --dry-run (no connection); --help
status, screenshot, logs without --follow: --timeout <1-120 seconds> (default 30)
Local forwarding ports: 1024-65535; target ports: 1-65535.

Uses QNX_TARGET_IP/USER, QNX_SSH_PORT/KEY/MACS/CONNECT_TIMEOUT and QNX_TARGET_DIR.
Requires OpenSSH key/agent authentication and an already verified known-host key.
SSH configuration files and QNX_SSH_OPTS are not used by these restricted tools.
No boot, install, restart, root escalation or automatic inspector activation.
`;

/** @typedef {{ executable: string, args: string[], cwd: string,
 * timeoutMs: number | null, output: string | null, dryRun: boolean }} DevicePlan */

/** @param {string | undefined} value @param {number} min @param {number} max
 * @param {string} label */
function integer(value, min, max, label) {
  if (!value || !/^[1-9][0-9]{0,4}$/.test(value)) {
    throw new Error(`${label} must be an integer from ${min} to ${max}.`);
  }
  const number = Number(value);
  if (number < min || number > max) {
    throw new Error(`${label} must be an integer from ${min} to ${max}.`);
  }
  return number;
}

/** Pure argument planning: never starts SSH or writes an output file.
 * @param {string[]} argv @param {string} workspace @param {string} cwd
 * @returns {DevicePlan | null} */
export function planDeviceCommand(argv, workspace, cwd) {
  const { values, positionals, tokens } = parseArgs({
    args: argv,
    strict: true,
    allowPositionals: true,
    tokens: true,
    options: {
      help: { type: 'boolean', short: 'h' },
      'dry-run': { type: 'boolean' },
      follow: { type: 'boolean' },
      out: { type: 'string' },
      timeout: { type: 'string' },
      'local-port': { type: 'string' },
      'remote-port': { type: 'string' },
    },
  });
  const seen = new Set();
  for (const token of tokens) {
    if (token.kind !== 'option') continue;
    if (seen.has(token.name)) throw new Error(`Duplicate option: --${token.name}.`);
    seen.add(token.name);
  }
  if (values.help || argv.length === 0) return null;
  const command = positionals[0];
  /** @type {Record<string, string[]>} */
  const allowed = {
    status: ['timeout'],
    logs: ['follow', 'timeout'],
    screenshot: ['out', 'timeout'],
    'port-forward': ['local-port', 'remote-port'],
  };
  if (positionals.length !== 1 || !Object.hasOwn(allowed, command)) {
    throw new Error('Choose status, logs, screenshot or port-forward. See --help.');
  }
  for (const option of Object.keys(values)) {
    if (option !== 'dry-run' && !allowed[command].includes(option)) {
      throw new Error(`${command} does not accept --${option}.`);
    }
  }
  const streaming = command === 'port-forward' || (command === 'logs' && values.follow);
  if (streaming && values.timeout !== undefined) {
    throw new Error('Use Ctrl-C to stop a foreground stream; --timeout is for finite commands.');
  }
  const timeoutMs = streaming ? null : integer(values.timeout ?? '30', 1, 120, '--timeout') * 1000;
  const args = [path.join(workspace, 'scripts/qnx-device.sh'), command];
  let output = null;
  if (command === 'logs' && values.follow) args[1] = 'logs-follow';
  if (command === 'port-forward') {
    args.push(
      String(integer(values['local-port'], 1024, 65535, '--local-port')),
      String(integer(values['remote-port'], 1, 65535, '--remote-port'))
    );
  }
  if (command === 'screenshot') {
    if (!values.out || !values.out.trim() || !values.out.endsWith('.bmp')) {
      throw new Error('screenshot requires --out naming a new .bmp file.');
    }
    output = path.resolve(cwd, values.out);
    args.push(randomBytes(12).toString('hex'));
  }
  return {
    executable: 'bash',
    args,
    cwd: workspace,
    timeoutMs,
    output,
    dryRun: !!values['dry-run'],
  };
}

/** @param {string} output */
async function requireNewOutput(output) {
  try {
    await lstat(output);
    throw new Error('Screenshot destination already exists; choose a new file.');
  } catch (error) {
    if (!(error instanceof Error && 'code' in error && error.code === 'ENOENT')) throw error;
  }
  if (!(await stat(path.dirname(output))).isDirectory()) {
    throw new Error('Screenshot parent must be an existing directory.');
  }
}

/** @param {DevicePlan} plan @param {NodeJS.ProcessEnv} [env] @returns {Promise<number>} */
export async function executeDevicePlan(plan, env = process.env) {
  if (plan.dryRun) {
    console.log(JSON.stringify(plan, null, 2));
    return 0;
  }
  if (plan.output) await requireNewOutput(plan.output);
  return new Promise((resolve, reject) => {
    const child = spawn(plan.executable, plan.args, {
      cwd: plan.cwd,
      env,
      shell: false,
      stdio: ['ignore', plan.output ? 'pipe' : 'inherit', 'inherit'],
    });
    /** @type {Buffer[]} */
    const chunks = [];
    let bytes = 0;
    let forcedCode = 0;
    let failure = '';
    /** @type {NodeJS.Timeout | undefined} */
    let killTimer;
    /** @param {NodeJS.Signals} signal @param {number} code */
    const stop = (signal, code) => {
      if (forcedCode !== 0) return;
      forcedCode = code;
      child.kill(signal);
      killTimer = setTimeout(() => child.kill('SIGKILL'), 1000);
    };
    const interrupt = () => stop('SIGINT', 130);
    const terminate = () => stop('SIGTERM', 143);
    process.on('SIGINT', interrupt);
    process.on('SIGTERM', terminate);
    /** @type {NodeJS.Timeout | undefined} */
    let timer;
    if (plan.timeoutMs !== null) {
      timer = setTimeout(() => {
        failure = 'Device command timed out; no readiness result was recorded.';
        stop('SIGTERM', 124);
      }, plan.timeoutMs);
    }
    const cleanup = () => {
      clearTimeout(timer);
      clearTimeout(killTimer);
      process.off('SIGINT', interrupt);
      process.off('SIGTERM', terminate);
    };
    child.stdout?.on('data', (/** @type {Buffer} */ chunk) => {
      bytes += chunk.length;
      if (bytes > MAX_CAPTURE_BYTES) {
        failure = 'Screenshot exceeds the 32 MiB capture limit.';
        chunks.length = 0;
        stop('SIGTERM', 1);
      } else if (!forcedCode) chunks.push(chunk);
    });
    child.once('error', (error) => {
      cleanup();
      reject(error);
    });
    child.once('close', (code, signal) => {
      cleanup();
      const finish = async () => {
        if (failure) console.error(failure);
        if (forcedCode) return forcedCode;
        if (code !== 0) return code ?? (signal === 'SIGINT' ? 130 : signal === 'SIGTERM' ? 143 : 1);
        if (plan.output) {
          const image = Buffer.concat(chunks);
          if (
            image.length < 26 ||
            image[0] !== 0x42 ||
            image[1] !== 0x4d ||
            image.readUInt32LE(2) !== image.length ||
            image.readUInt32LE(14) < 12 ||
            14 + image.readUInt32LE(14) > image.readUInt32LE(10) ||
            image.readUInt32LE(10) >= image.length
          ) {
            throw new Error('Target returned an incomplete or invalid BMP; nothing was written.');
          }
          // Exclusive creation also rejects a file/symlink introduced after the preflight.
          await writeFile(plan.output, image, { flag: 'wx', mode: 0o600 });
          console.log(`Screenshot saved: ${plan.output}`);
        }
        return 0;
      };
      finish().then(resolve, reject);
    });
  });
}

/** @param {string[]} argv */
export async function main(argv) {
  const plan = planDeviceCommand(argv, root, process.cwd());
  if (!plan) {
    console.log(help);
    return 0;
  }
  const manifest = JSON.parse(await readFile(path.join(root, 'package.json'), 'utf8'));
  if (manifest.name !== 'react-native-qnx-monorepo') {
    throw new Error('These device tools require the react-native-qnx source checkout.');
  }
  return executeDevicePlan(plan);
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    process.exitCode = await main(process.argv.slice(2));
  } catch (error) {
    console.error(`qnx-device: ${error instanceof Error ? error.message : error}`);
    process.exitCode = 1;
  }
}
