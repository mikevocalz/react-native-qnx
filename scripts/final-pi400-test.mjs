#!/usr/bin/env node
// Issue #43: supervised Pi 400 acceptance. No flashing, rebooting or release promotion.
import { spawn } from 'node:child_process';
import { createHash } from 'node:crypto';
import { createReadStream } from 'node:fs';
import { lstat, mkdir, open, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { parseArgs } from 'node:util';
import { fileURLToPath } from 'node:url';
import { nativeConfiguration } from '../packages/react-native-qnx-cli/src/prepare-native.mjs';
import { planNativeBuild } from '../packages/react-native-qnx-cli/src/build-native.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const stages = Object.freeze([
  'pi400-preflight',
  'qnx-screen-deploy',
  'hermes-nitro-expo-probe',
  'full-fabric-qt-consumer-crossbuild',
  'physical-consumer-render',
]);
const help = [
  'Issue #43 / QNX-RN-008: final physical Pi 400 acceptance',
  '  node scripts/final-pi400-test.mjs plan',
  '  node scripts/final-pi400-test.mjs execute --consumer-project /absolute/path/to/app',
  'Execute requires RNQNX_FINAL_TEST_CONFIRM=pi400, RNQNX_PI_BOARD=pi400,',
  'a licensed QNX SDP, pinned Hermes HBC and verified SSH host key.',
  'No flash, reboot, implicit root access or release-gate promotion.',
].join('\n');

/** @param {string} name @param {unknown} value */
function token(name, value) {
  if (typeof value !== 'string' || !/^[A-Za-z0-9._-]+$/.test(value)) {
    throw new Error(name + ' contains unsupported characters.');
  }
  return value;
}

/** @param {string[]} command @param {number} maxMs @returns {Promise<string>} */
async function commandRun(command, maxMs) {
  const [bin, ...args] = command;
  return new Promise((resolve, reject) => {
    const child = spawn(bin, args, {
      cwd: root,
      shell: false,
      env: process.env,
      stdio: ['ignore', 'pipe', 'pipe'],
    });
    let output = '';
    let errors = '';
    let bytes = 0;
    let timedOut = false;
    const timer = setTimeout(() => {
      timedOut = true;
      child.kill('SIGTERM');
    }, maxMs);
    /** @param {Buffer} data @param {boolean} isError */
    const collect = (data, isError) => {
      bytes += data.length;
      if (bytes > 8 * 1024 * 1024) child.kill('SIGTERM');
      if (isError) errors += data.toString('utf8');
      else output += data.toString('utf8');
    };
    child.stdout?.on('data', (data) => collect(data, false));
    child.stderr?.on('data', (data) => collect(data, true));
    child.once('error', (error) => {
      clearTimeout(timer);
      reject(error);
    });
    child.once('close', (code, signal) => {
      clearTimeout(timer);
      if (timedOut || bytes > 8 * 1024 * 1024)
        reject(new Error(bin + ' timed out or overflowed output'));
      else if (code !== 0)
        reject(new Error(bin + ' failed ' + (signal ?? code) + ': ' + errors.slice(-900)));
      else resolve(output);
    });
  });
}

/** @param {string} name @param {string[]} argv @param {number} ms */
async function runStage(name, argv, ms) {
  const started = new Date().toISOString();
  try {
    const output = await commandRun(argv, ms);
    return {
      name,
      status: 'passed',
      started,
      ended: new Date().toISOString(),
      output: output.slice(-1200),
    };
  } catch (error) {
    return {
      name,
      status: 'failed',
      started,
      ended: new Date().toISOString(),
      error: String(error).slice(0, 1500),
    };
  }
}

/** @param {string} file @param {number} limit */
async function fingerprint(file, limit) {
  const st = await lstat(file);
  if (!st.isFile() || st.isSymbolicLink() || st.size < 1 || st.size > limit) {
    throw new Error('Physical artifact is not a bounded regular file.');
  }
  const digest = createHash('sha256');
  for await (const data of createReadStream(file)) digest.update(data);
  return { bytes: st.size, sha256: digest.digest('hex') };
}

/** @param {string[]} argv */
export async function main(argv) {
  const { positionals, values, tokens } = parseArgs({
    args: argv,
    strict: true,
    allowPositionals: true,
    tokens: true,
    options: { help: { type: 'boolean', short: 'h' }, 'consumer-project': { type: 'string' } },
  });
  const found = new Set();
  for (const entry of tokens) {
    if (entry.kind !== 'option') continue;
    if (found.has(entry.name)) throw new Error('Duplicate option --' + entry.name);
    found.add(entry.name);
  }
  if (values.help || argv.length === 0) {
    console.log(help);
    return 0;
  }
  if (positionals.length !== 1 || !['plan', 'execute'].includes(positionals[0])) {
    throw new Error('Choose plan or execute.');
  }
  if (positionals[0] === 'plan') {
    if (values['consumer-project']) throw new Error('Plan does not accept app paths.');
    console.log(
      JSON.stringify(
        {
          issue: 43,
          mode: 'plan',
          board: 'pi400',
          image: 'custom CTI 8.0.5',
          stages: stages.map((name) => ({ name, status: 'not-run' })),
          physicalVerified: false,
          stockQstiQualified: false,
          releaseAuthorized: false,
        },
        null,
        2
      )
    );
    return 0;
  }
  if (process.env.RNQNX_FINAL_TEST_CONFIRM !== 'pi400' || process.env.RNQNX_PI_BOARD !== 'pi400') {
    throw new Error('Explicit Pi 400 consent and board selection are required.');
  }
  const app = values['consumer-project'];
  if (!app || !path.isAbsolute(app)) throw new Error('Use an absolute --consumer-project.');
  if (!process.env.QNX_HOST || !process.env.QNX_TARGET)
    throw new Error('QNX SDP environment not loaded.');
  const { configuration } = await nativeConfiguration(app, 'qnx', 'build/qnx/app.hbc');
  const build = await planNativeBuild(app, 'qnx');
  const user = token('QNX_TARGET_USER', process.env.QNX_TARGET_USER ?? 'qnxuser');
  const host = token('QNX_TARGET_IP', process.env.QNX_TARGET_IP ?? 'qnxpi.local');
  const targetRoot = process.env.QNX_TARGET_DIR ?? '/data/home/' + user + '/rnqnx';
  if (!/^\/[A-Za-z0-9._/-]+$/.test(targetRoot) || targetRoot.split('/').includes('..')) {
    throw new Error('Target root must be a confined absolute path.');
  }
  const target = user + '@' + host;
  const options = [
    '-o',
    'BatchMode=yes',
    '-o',
    'StrictHostKeyChecking=yes',
    '-o',
    'ConnectTimeout=10',
    '-o',
    'MACs=hmac-sha2-256',
  ];
  const stamp = new Date()
    .toISOString()
    .replaceAll(/[^0-9]/g, '')
    .slice(0, 14);
  const evidence = path.join(root, 'artifacts', 'pi-final', stamp);
  await mkdir(evidence, { recursive: true, mode: 0o700 });
  /** @type {Array<Record<string, unknown>>} */
  const results = [];
  /** @type {Record<string, any>} */
  const record = {
    schemaVersion: 1,
    issue: 43,
    board: 'pi400',
    image: 'custom CTI 8.0.5 (operator label)',
    sourceCommit: (await commandRun(['git', 'rev-parse', 'HEAD'], 10000)).trim(),
    bundle: await fingerprint(configuration.bundle, 256 * 1024 * 1024),
    startedAt: new Date().toISOString(),
    results,
    releaseAuthorized: false,
    stockQstiQualified: false,
    coldBootQualified: false,
  };
  const save = async () =>
    writeFile(path.join(evidence, 'result.json'), JSON.stringify(record, null, 2) + '\n');
  /** @param {string} name @param {string[]} cmd @param {number} ms @returns {Promise<string>} */
  const required = async (name, cmd, ms) => {
    const result = await runStage(name, cmd, ms);
    results.push(result);
    await save();
    if (result.status !== 'passed') throw new Error('Pi 400 test failed: ' + name);
    return result.output ?? '';
  };
  try {
    await required('pi400-preflight', ['bash', 'scripts/pi-preflight.sh'], 120000);
    await required('qnx-screen-deploy', ['bash', 'scripts/install-pi.sh'], 1200000);
    await required('hermes-nitro-expo-probe', ['bash', 'scripts/validate-runtime-qnx.sh'], 2400000);
    await required(
      'full-fabric-qt-consumer-crossbuild',
      [
        'node',
        'packages/react-native-qnx-cli/src/bin.mjs',
        'project',
        'build-native',
        '--project',
        app,
        '--platform',
        'qnx',
      ],
      2400000
    );
    record.executable = await fingerprint(build.output, 1024 * 1024 * 1024);
    const remote = targetRoot + '/final-' + configuration.appId + '-' + stamp;
    await required(
      'isolated-remote-dir',
      ['ssh', ...options, target, "mkdir '" + remote + "'"],
      30000
    );
    await required(
      'native-executable-upload',
      ['scp', ...options, build.output, target + ':' + remote + '/app'],
      180000
    );
    await required(
      'matched-bundle-upload',
      ['scp', ...options, configuration.bundle, target + ':' + remote + '/app.bundle.js'],
      180000
    );
    const remoteCommand =
      "cd '" + remote + "' && chmod 700 ./app && LD_LIBRARY_PATH=. ./app --screenshot final.png";
    const output = await required(
      'physical-consumer-render',
      ['ssh', ...options, target, remoteCommand],
      90000
    );
    if (!output.includes('RNQNX_PLAYGROUND_SCREENSHOT'))
      throw new Error('Physical render marker missing.');
    const capture = path.join(evidence, 'physical-consumer.png');
    await required(
      'physical-capture-download',
      ['scp', ...options, target + ':' + remote + '/final.png', capture],
      90000
    );
    const handle = await open(capture, 'r');
    let signature;
    try {
      const head = Buffer.alloc(8);
      const { bytesRead } = await handle.read(head, 0, 8, 0);
      signature = bytesRead === 8 ? head.toString('hex') : '';
    } finally {
      await handle.close();
    }
    if (signature !== '89504e470d0a1a0a') throw new Error('Physical capture is not PNG.');
    record.capture = await fingerprint(capture, 32 * 1024 * 1024);
    record.physicalRuntimeCapture = true;
    record.finishedAt = new Date().toISOString();
    await save();
    console.log(
      JSON.stringify(
        {
          outcome: 'physical-runtime-capture-passed',
          evidence,
          releaseAuthorized: false,
          stockQstiQualified: false,
          coldBootQualified: false,
        },
        null,
        2
      )
    );
    return 0;
  } catch (error) {
    record.error = String(error);
    record.finishedAt = new Date().toISOString();
    await save();
    throw error;
  }
}
const invoked = process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url);
if (invoked) {
  main(process.argv.slice(2)).catch((error) => {
    console.error('RNQNX_PI400_FINAL_TEST_FAILED:', error.message);
    process.exitCode = 1;
  });
}
