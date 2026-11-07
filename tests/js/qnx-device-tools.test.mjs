import assert from 'node:assert/strict';
import { spawn, spawnSync } from 'node:child_process';
import { cp, mkdir, mkdtemp, readFile, readdir, rm, symlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { setTimeout as delay } from 'node:timers/promises';
import { planDeviceCommand } from '../../packages/qnx-devtools/src/cli.mjs';

const root = process.cwd();
const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-device-tools-'));
let cases = 0;
/** @param {unknown} actual @param {unknown} expected @param {string} label */
function equal(actual, expected, label) {
  assert.deepEqual(actual, expected, label);
  cases += 1;
}
try {
  const cli = path.join(temporary, 'packages/qnx-devtools/src/cli.mjs');
  await mkdir(path.dirname(cli), { recursive: true });
  await cp(path.join(root, 'packages/qnx-devtools/src/cli.mjs'), cli);
  await mkdir(path.join(temporary, 'scripts/lib'), { recursive: true });
  await cp(path.join(root, 'scripts/qnx-device.sh'), path.join(temporary, 'scripts/qnx-device.sh'));
  await cp(
    path.join(root, 'scripts/lib/qnx-target.sh'),
    path.join(temporary, 'scripts/lib/qnx-target.sh')
  );
  await writeFile(
    path.join(temporary, 'package.json'),
    JSON.stringify({ name: 'react-native-qnx-monorepo' })
  );
  const bin = path.join(temporary, 'bin');
  const device = path.join(temporary, 'device');
  await mkdir(bin);
  await mkdir(device);
  const log = path.join(temporary, 'ssh.json');
  const pidFile = path.join(temporary, 'ssh.pid');
  // Run the real wrapper with an isolated SSH process fixture. Its remote
  // script runs locally against synthetic tools, never a real network/device.
  await writeFile(
    path.join(bin, 'ssh'),
    `#!${process.execPath}
const fs = require('node:fs');
const cp = require('node:child_process');
fs.writeFileSync(process.env.QNX_TEST_LOG, JSON.stringify(process.argv.slice(2)));
if (process.env.QNX_TEST_MODE === 'failure') process.exit(37);
if (process.env.QNX_TEST_MODE === 'hang') {
  fs.writeFileSync(process.env.QNX_TEST_PID, String(process.pid));
  process.on('SIGTERM', () => process.exit(0));
  process.on('SIGINT', () => process.exit(0));
  setInterval(() => {}, 1000);
} else if (process.argv.includes('-N')) {
  process.exit(0);
} else {
  const input = fs.readFileSync(0);
  const result = cp.spawnSync('/bin/sh', ['-c', process.argv.at(-1)], {
    input, env: process.env, maxBuffer: 64 * 1024 * 1024,
  });
  process.stderr.write(result.stderr || '');
  process.stdout.write(result.stdout || '', () => process.exit(result.status ?? 1));
}
`,
    { mode: 0o755 }
  );
  await writeFile(
    path.join(bin, 'uname'),
    '#!/bin/sh\nif [ "$1" = -s ]; then printf "%s\\n" "${QNX_TEST_OS:-QNX}"; else echo aarch64le; fi\n',
    { mode: 0o755 }
  );
  await writeFile(path.join(bin, 'slog2info'), '#!/bin/sh\necho "synthetic slog $*"\n', {
    mode: 0o755,
  });
  await writeFile(
    path.join(bin, 'screenshot'),
    `#!${process.execPath}
const fs = require('node:fs');
if (process.env.QNX_TEST_MODE === 'capture-failure') process.exit(9);
const bytes = Buffer.alloc(process.env.QNX_TEST_MODE === 'oversize' ? 33 * 1024 * 1024 : 58);
bytes.write('BM');
bytes.writeUInt32LE(bytes.length, 2);
bytes.writeUInt32LE(54, 10);
bytes.writeUInt32LE(40, 14);
if (process.env.QNX_TEST_MODE === 'bad-bmp') bytes.write('NO');
const image = process.env.QNX_TEST_MODE === 'truncated' ? bytes.subarray(0, 32) : bytes;
fs.writeFileSync(process.argv[2].slice('-file='.length), image);
`,
    { mode: 0o755 }
  );
  const env = {
    ...process.env,
    PATH: `${bin}${path.delimiter}${process.env.PATH}`,
    QNX_TARGET_IP: 'fixture.invalid',
    QNX_TARGET_USER: 'qnxuser',
    QNX_TARGET_DIR: device,
    QNX_SSH_PORT: '22',
    QNX_SSH_KEY: '',
    QNX_SSH_OPTS: '',
    QNX_SSH_CONNECT_TIMEOUT: '1',
    QNX_TEST_LOG: log,
    QNX_TEST_PID: pidFile,
    QNX_TEST_MODE: '',
    QNX_TEST_OS: 'QNX',
  };
  /** @param {string[]} args @param {NodeJS.ProcessEnv} [extra] */
  function run(args, extra = {}) {
    const result = spawnSync(process.execPath, [cli, ...args], {
      cwd: temporary,
      env: { ...env, ...extra },
      encoding: 'utf8',
      timeout: 10000,
      maxBuffer: 2 * 1024 * 1024,
    });
    assert.equal(result.error, undefined);
    return result;
  }
  equal(run(['--help']).status, 0, 'help works without connection');
  equal(run(['status', '--dry-run']).status, 0, 'dry-run succeeds');
  await assert.rejects(readFile(log), /ENOENT/);
  cases += 1;
  const status = run(['status']);
  equal(status.status, 0, status.stderr);
  assert.match(status.stdout, /os=QNX\narchitecture=aarch64le/);
  cases += 1;
  equal(run(['status'], { QNX_TEST_OS: 'Linux' }).status, 42, 'wrong OS rejected');
  equal(run(['logs']).stdout.trim(), 'synthetic slog', 'finite logs use slog2info');
  equal(run(['logs', '--follow']).stdout.trim(), 'synthetic slog -w', 'follow uses actual flag');
  equal(run(['logs'], { QNX_TEST_MODE: 'failure' }).status, 37, 'SSH status preserved');
  equal(
    run(['status', '--timeout', '1'], { QNX_TEST_MODE: 'hang' }).status,
    124,
    'timeout is failure'
  );
  const portArgs = ['port-forward', '--local-port', '9229', '--remote-port', '9229'];
  equal(run(portArgs).status, 0, 'forwarding wrapper');
  const sshArgs = JSON.parse(await readFile(log, 'utf8'));
  for (const option of [
    '127.0.0.1:9229:127.0.0.1:9229',
    'ExitOnForwardFailure=yes',
    'StrictHostKeyChecking=yes',
    'BatchMode=yes',
    'GatewayPorts=no',
    '-N',
    '-F',
  ]) {
    assert.ok(sshArgs.includes(option), option);
    cases += 1;
  }
  equal(sshArgs.at(-1), 'qnxuser@fixture.invalid', 'target identity');
  equal(
    run(['status'], { QNX_SSH_OPTS: '-o StrictHostKeyChecking=no' }).status,
    2,
    'extra SSH options rejected'
  );
  equal(
    run(['status'], { QNX_SSH_PORT: '9999999999999999999999' }).status,
    2,
    'oversized port rejected'
  );
  const unsafeTarget = run(['status'], { QNX_TARGET_IP: 'host;touch-bad' });
  equal(unsafeTarget.status, 1, 'target injection rejected');

  const output = path.join(temporary, 'screen.bmp');
  equal(run(['screenshot', '--out', output]).status, 0, 'valid capture');
  equal((await readFile(output)).length, 58, 'captured exact bytes');
  equal(await readdir(device), [], 'owned remote temporary directory cleaned');
  equal(run(['screenshot', '--out', output]).status, 1, 'no overwrite');
  const link = path.join(temporary, 'link.bmp');
  await symlink(output, link);
  equal(run(['screenshot', '--out', link]).status, 1, 'symlink cannot overwrite');
  equal((await readFile(output)).length, 58, 'original capture untouched');
  for (const mode of ['capture-failure', 'bad-bmp', 'truncated', 'oversize']) {
    const destination = path.join(temporary, `${mode}.bmp`);
    const captured = run(['screenshot', '--out', destination], { QNX_TEST_MODE: mode });
    assert.notEqual(captured.status, 0);
    cases += 1;
    if (mode === 'oversize') {
      assert.match(captured.stderr, /32 MiB capture limit/);
      cases += 1;
    }
    if (mode === 'truncated') {
      assert.match(captured.stderr, /incomplete or invalid BMP/);
      cases += 1;
    }
    await assert.rejects(readFile(destination), /ENOENT/);
    cases += 1;
    equal(await readdir(device), [], `${mode}: remote cleanup`);
  }
  for (const args of [
    ['unknown'],
    ['status', 'extra'],
    ['status', '--out', 'x.bmp'],
    ['logs', '--timeout', '0'],
    ['logs', '--follow', '--timeout', '1'],
    ['logs', '--timeout', '121'],
    ['logs', '--timeout', '1', '--timeout', '2'],
    ['screenshot'],
    ['screenshot', '--out', 'x.png'],
    ['port-forward'],
    ['port-forward', '--local-port', '22', '--remote-port', '9229'],
    ['port-forward', '--local-port', '9229', '--remote-port', '0'],
    ['port-forward', '--local-port', '9229', '--remote-port', '4;bad'],
  ]) {
    assert.throws(() => planDeviceCommand(args, temporary, temporary));
    cases += 1;
  }
  // Signal propagation targets the actual owned SSH process, not an unowned
  // daemon or a controller that outlives this CLI invocation.
  await rm(pidFile, { force: true });
  const child = spawn(process.execPath, [cli, 'logs', '--follow'], {
    cwd: temporary,
    env: { ...env, QNX_TEST_MODE: 'hang' },
    stdio: 'ignore',
  });
  const closed = new Promise((resolve, reject) => {
    child.once('error', reject);
    child.once('close', (code, signal) => resolve({ code, signal }));
  });
  const deadline = setTimeout(() => child.kill('SIGKILL'), 5000);
  try {
    for (let i = 0; i < 100; i += 1) {
      try {
        await readFile(pidFile);
        break;
      } catch {
        await delay(20);
      }
    }
    const sshPid = Number(await readFile(pidFile, 'utf8'));
    child.kill('SIGTERM');
    equal(await closed, { code: 143, signal: null }, 'foreground stop preserves signal failure');
    assert.throws(() => process.kill(sshPid, 0), /ESRCH/);
    cases += 1;
  } finally {
    clearTimeout(deadline);
    child.kill('SIGKILL');
  }
  console.log(`qnx-device-tools: ${cases} checks passed (synthetic SSH/remote shell, no device).`);
} finally {
  await rm(temporary, { recursive: true, force: true });
}
