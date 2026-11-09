#!/usr/bin/env node
// Issue #43: supervised Pi 400 acceptance. No flashing, rebooting or release promotion.
import { spawn } from 'node:child_process';
import { createHash } from 'node:crypto';
import { createReadStream } from 'node:fs';
import { lstat, mkdir, mkdtemp, open, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { parseArgs } from 'node:util';
import { fileURLToPath } from 'node:url';
import { planNativeBuild } from '../packages/react-native-qnx-cli/src/build-native.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const stages = Object.freeze([
  'non-root-target',
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
  if (typeof value !== 'string' || !/^[A-Za-z0-9_][A-Za-z0-9._-]*$/.test(value)) {
    throw new Error(name + ' contains unsupported characters.');
  }
  return value;
}

/** @param {string[]} command @param {number} maxMs @param {NodeJS.ProcessEnv} [env] @returns {Promise<string>} */
export async function commandRun(command, maxMs, env = process.env) {
  if (!['linux', 'darwin'].includes(process.platform)) {
    throw new Error('Physical stage supervision requires Linux or macOS.');
  }
  if (!command.length || !Number.isSafeInteger(maxMs) || maxMs < 1 || maxMs > 2147483647) {
    throw new Error('A command and a bounded positive timeout are required.');
  }
  const [bin, ...args] = command;
  return new Promise((resolve, reject) => {
    const child = spawn(bin, args, {
      cwd: root,
      shell: false,
      env,
      detached: true,
      stdio: ['ignore', 'pipe', 'pipe'],
    });
    let output = '';
    let errors = '';
    let bytes = 0;
    let settled = false;
    /** @type {Error | undefined} */
    let failure;
    /** @type {ReturnType<typeof setTimeout> | undefined} */
    let killTimer;
    /** @param {NodeJS.Signals} signal */
    const signalGroup = (signal) => {
      if (!child.pid) return;
      try {
        process.kill(-child.pid, signal);
      } catch (error) {
        if (!(error instanceof Error && 'code' in error && error.code === 'ESRCH')) {
          failure ??= new Error('Could not terminate stage process group: ' + String(error));
        }
      }
    };
    /** @param {Error} [error] */
    const finish = (error) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      clearTimeout(killTimer);
      process.off('SIGINT', interrupt);
      process.off('SIGTERM', terminate);
      signalGroup('SIGKILL');
      child.stdout?.destroy();
      child.stderr?.destroy();
      child.unref();
      if (failure || error) reject(failure ?? error);
      else resolve(output);
    };
    /** @param {Error} error */
    const stop = (error) => {
      if (failure || settled) return;
      failure = error;
      signalGroup('SIGTERM');
      killTimer = setTimeout(() => finish(failure), 250);
    };
    const interrupt = () => stop(new Error(bin + ' interrupted by SIGINT'));
    const terminate = () => stop(new Error(bin + ' interrupted by SIGTERM'));
    process.on('SIGINT', interrupt);
    process.on('SIGTERM', terminate);
    const timer = setTimeout(() => stop(new Error(bin + ' timed out')), maxMs);
    /** @param {Buffer} data @param {boolean} isError */
    const collect = (data, isError) => {
      if (failure || settled) return;
      bytes += data.length;
      if (bytes > 8 * 1024 * 1024) {
        stop(new Error(bin + ' overflowed output'));
        return;
      }
      if (isError) errors += data.toString('utf8');
      else output += data.toString('utf8');
    };
    child.stdout?.on('data', (data) => collect(data, false));
    child.stderr?.on('data', (data) => collect(data, true));
    child.once('error', (error) => finish(error));
    child.once('close', (code, signal) => {
      finish(
        code === 0
          ? undefined
          : new Error(bin + ' failed ' + (signal ?? code) + ': ' + errors.slice(-900))
      );
    });
  });
}

/** @param {string} name @param {string[]} argv @param {number} ms @param {NodeJS.ProcessEnv} env @param {(output: string) => Promise<void>} validate */
export async function runStage(name, argv, ms, env, validate) {
  const started = new Date().toISOString();
  try {
    const output = await commandRun(argv, ms, env);
    await validate(output);
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

/** @param {NodeJS.ProcessEnv} [source] */
export async function targetConnection(source = process.env) {
  const user = token('QNX_TARGET_USER', source.QNX_TARGET_USER ?? 'qnxuser');
  if (user === 'root' || !/^[A-Za-z_]/.test(user)) {
    throw new Error('A non-root named QNX_TARGET_USER is required.');
  }
  const host = token('QNX_TARGET_IP', source.QNX_TARGET_IP ?? 'qnxpi.local');
  if (!/^[A-Za-z0-9][A-Za-z0-9.-]*$/.test(host)) throw new Error('Invalid target hostname.');
  if (source.QNX_SSH_OPTS?.trim()) {
    throw new Error(
      'QNX_SSH_OPTS is not allowed in physical qualification; use verified SSH config, QNX_SSH_KEY and QNX_SSH_PORT.'
    );
  }
  const targetRoot = source.QNX_TARGET_DIR ?? '/data/home/' + user + '/rnqnx';
  if (!/^\/[A-Za-z0-9._/-]+$/.test(targetRoot) || targetRoot.split('/').includes('..')) {
    throw new Error('Target root must be a confined absolute path.');
  }
  const fontDirectory = source.RNQNX_PI_QT_FONTDIR;
  if (
    fontDirectory !== undefined &&
    (!/^\/[A-Za-z0-9._/-]+$/.test(fontDirectory) || fontDirectory.split('/').includes('..'))
  ) {
    throw new Error('Qt font directory must be a confined absolute path.');
  }
  const port = source.QNX_SSH_PORT ?? '22';
  if (!/^[0-9]+$/.test(port) || Number(port) < 1 || Number(port) > 65535) {
    throw new Error('Invalid QNX SSH port.');
  }
  const options = [
    '-o',
    'IdentitiesOnly=yes',
    '-o',
    'BatchMode=yes',
    '-o',
    'StrictHostKeyChecking=yes',
    '-o',
    'ConnectTimeout=10',
    '-o',
    'MACs=hmac-sha2-256',
  ];
  const sshOptions = [...options];
  const scpOptions = [...options];
  if (port !== '22') {
    sshOptions.push('-p', port);
    scpOptions.push('-P', port);
  }
  const key = source.QNX_SSH_KEY;
  if (key) {
    const keyStat = await lstat(key);
    if (!keyStat.isFile() || keyStat.isSymbolicLink()) {
      throw new Error('QNX_SSH_KEY must be a regular private key file.');
    }
    sshOptions.push('-i', key);
    scpOptions.push('-i', key);
  }
  const env = {
    ...source,
    QNX_TARGET_USER: user,
    QNX_TARGET_IP: host,
    QNX_TARGET_DIR: targetRoot,
    QNX_SSH_PORT: port,
    QNX_SSH_OPTS: '-o IdentitiesOnly=yes -o StrictHostKeyChecking=yes',
    QNX_SSH_BATCH_MODE: 'yes',
    QNX_SSH_CONNECT_TIMEOUT: '10',
    QNX_SSH_MACS: 'hmac-sha2-256',
  };
  const libraryDirectories = source.RNQNX_PI_LIBRARY_PATH?.split(':') ?? [];
  if (
    source.RNQNX_PI_LIBRARY_PATH !== undefined &&
    (source.RNQNX_PI_LIBRARY_PATH.length > 4096 ||
      libraryDirectories.length > 16 ||
      libraryDirectories.some(
        (directory) =>
          !/^\/[A-Za-z0-9._/-]+$/.test(directory) || directory.split('/').includes('..')
      ))
  ) {
    throw new Error('Runtime library path must contain confined absolute directories.');
  }
  const pluginDirectory = source.RNQNX_PI_QT_PLUGIN_PATH;
  if (
    pluginDirectory !== undefined &&
    (!/^\/[A-Za-z0-9._/-]+$/.test(pluginDirectory) ||
      pluginDirectory.length > 4096 ||
      pluginDirectory.split('/').includes('..'))
  ) {
    throw new Error('Qt plugin directory must be a confined absolute path.');
  }
  return {
    target: user + '@' + host,
    targetRoot,
    fontDirectory,
    libraryDirectories,
    libraryPath: ['.', ...libraryDirectories].join(':'),
    pluginDirectory,
    sshOptions,
    scpOptions,
    env,
  };
}

/** @param {string} capture */
export async function captureFingerprint(capture) {
  const result = await fingerprint(capture, 32 * 1024 * 1024);
  const handle = await open(capture, 'r');
  try {
    const head = Buffer.alloc(8);
    const { bytesRead } = await handle.read(head, 0, 8, 0);
    if (bytesRead !== 8 || head.toString('hex') !== '89504e470d0a1a0a') {
      throw new Error('Physical capture is not PNG.');
    }
  } finally {
    await handle.close();
  }
  return result;
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
  const {
    target,
    targetRoot,
    fontDirectory,
    libraryDirectories,
    libraryPath,
    pluginDirectory,
    sshOptions,
    scpOptions,
    env,
  } = await targetConnection();
  const app = values['consumer-project'];
  if (!app || !path.isAbsolute(app)) throw new Error('Use an absolute --consumer-project.');
  if (!process.env.QNX_HOST || !process.env.QNX_TARGET)
    throw new Error('QNX SDP environment not loaded.');
  const build = await planNativeBuild(app, 'qnx');
  const { configuration } = build;
  const stamp = new Date()
    .toISOString()
    .replaceAll(/[^0-9]/g, '')
    .slice(0, 14);
  const evidenceRoot = path.join(root, 'artifacts', 'pi-final');
  await mkdir(evidenceRoot, { recursive: true, mode: 0o700 });
  const evidence = await mkdtemp(path.join(evidenceRoot, stamp + '-'));
  /** @type {Array<Record<string, unknown>>} */
  const results = [];
  /** @type {Record<string, any>} */
  const record = {
    schemaVersion: 1,
    issue: 43,
    board: 'pi400',
    image: 'custom CTI 8.0.5 (operator label)',
    sourceCommit: (await commandRun(['git', 'rev-parse', 'HEAD'], 10000, env)).trim(),
    qtFontDirectory: fontDirectory ?? null,
    runtimeLibraryPath: libraryPath,
    qtPluginDirectory: pluginDirectory ?? null,
    bundle: await fingerprint(configuration.bundle, 256 * 1024 * 1024),
    startedAt: new Date().toISOString(),
    results,
    releaseAuthorized: false,
    stockQstiQualified: false,
    coldBootQualified: false,
  };
  const save = async () =>
    writeFile(path.join(evidence, 'result.json'), JSON.stringify(record, null, 2) + '\n');
  /** @param {string} name @param {string[]} cmd @param {number} ms @param {(output: string) => Promise<void>} [validate] @returns {Promise<string>} */
  const required = async (name, cmd, ms, validate = async () => {}) => {
    const result = await runStage(name, cmd, ms, env, validate);
    results.push(result);
    await save();
    if (result.status !== 'passed') throw new Error('Pi 400 test failed: ' + name);
    return result.output ?? '';
  };
  const hermes = path.join(root, 'build/hermes-qnx-aarch64/lib/libhermesvmlean.so');
  const fontCheck = fontDirectory
    ? " && test -d '" +
      fontDirectory +
      "' && test -x '" +
      fontDirectory +
      "' && { font_ok=; for font in '" +
      fontDirectory +
      "'/*.ttf '" +
      fontDirectory +
      "'/*.otf '" +
      fontDirectory +
      "'/*.pfa '" +
      fontDirectory +
      '\'/*.pfb; do if test -f "$font" && test -r "$font"; then font_ok=yes; break; fi; done; ' +
      'test "$font_ok" = yes && printf "RNQNX_QT_FONTDIR_VERIFIED\\n"; }'
    : '';
  const libraryCheck = libraryDirectories
    .map(
      (directory) =>
        " && test -d '" +
        directory +
        "' && test -r '" +
        directory +
        "' && test -x '" +
        directory +
        "'"
    )
    .join('');
  const pluginCheck = pluginDirectory
    ? " && test -d '" +
      pluginDirectory +
      "' && test -x '" +
      pluginDirectory +
      "' && test -f '" +
      pluginDirectory +
      "/platforms/libqqnx.so' && test -r '" +
      pluginDirectory +
      "/platforms/libqqnx.so'"
    : '';
  try {
    await required(
      'non-root-target',
      [
        'ssh',
        ...sshOptions,
        target,
        'uid=$(id -u) && test "$uid" -gt 0 && test "$(uname -s)" = QNX && test "$(uname -m)" = RaspberryPi400 && test "$(uname -p)" = aarch64le && printf "RNQNX_TARGET_UID=%s\\n" "$uid"' +
          fontCheck +
          libraryCheck +
          pluginCheck,
      ],
      30000
    );
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
      2400000,
      async () => {
        record.executable = await fingerprint(build.output, 1024 * 1024 * 1024);
        record.hermesRuntime = await fingerprint(hermes, 256 * 1024 * 1024);
      }
    );
    const remote = targetRoot + '/final-' + configuration.appId + '-' + path.basename(evidence);
    await required(
      'isolated-remote-dir',
      ['ssh', ...sshOptions, target, "mkdir '" + remote + "'"],
      30000
    );
    await required(
      'hermes-runtime-upload',
      ['scp', ...scpOptions, hermes, target + ':' + remote + '/libhermesvmlean.so'],
      180000
    );
    await required(
      'native-executable-upload',
      ['scp', ...scpOptions, build.output, target + ':' + remote + '/app'],
      180000
    );
    await required(
      'matched-bundle-upload',
      ['scp', ...scpOptions, configuration.bundle, target + ':' + remote + '/app.bundle.js'],
      180000
    );
    const remoteCommand =
      "cd '" +
      remote +
      "' && chmod 700 ./app && LD_LIBRARY_PATH=" +
      libraryPath +
      ' ' +
      (fontDirectory ? "QT_QPA_FONTDIR='" + fontDirectory + "' " : '') +
      (pluginDirectory
        ? "QT_PLUGIN_PATH='" +
          pluginDirectory +
          "' QT_QPA_PLATFORM_PLUGIN_PATH='" +
          pluginDirectory +
          "/platforms' "
        : '') +
      './app --screenshot final.png';
    await required(
      'physical-consumer-render',
      ['ssh', ...sshOptions, target, remoteCommand],
      90000,
      async (output) => {
        if (!output.includes('RNQNX_PLAYGROUND_SCREENSHOT')) {
          throw new Error('Physical render marker missing.');
        }
      }
    );
    const capture = path.join(evidence, 'physical-consumer.png');
    await required(
      'physical-capture-download',
      ['scp', ...scpOptions, target + ':' + remote + '/final.png', capture],
      90000,
      async () => {
        record.capture = await captureFingerprint(capture);
      }
    );
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
    record.remoteCleanupVerified = false;
    record.remoteSideEffects = 'unknown; operator inspection required before retry';
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
