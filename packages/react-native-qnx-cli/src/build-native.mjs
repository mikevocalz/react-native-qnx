import { createHash } from 'node:crypto';
import { createReadStream } from 'node:fs';
import { spawn } from 'node:child_process';
import { lstat, readFile, realpath, mkdir } from 'node:fs/promises';
import path from 'node:path';
import { parseArgs } from 'node:util';
import { nativeConfiguration } from './prepare-native.mjs';
import { verifyPreparedIdentity } from './app-identity.mjs';
import { readWindowDescriptor, windowChromeCMakeFlags } from './window-descriptor.mjs';

const platformRoot = path.resolve(import.meta.dirname, '../../..');
const HELP = [
  'React Native QNX - build an external native consumer application',
  '  project build-native --project <directory> [--platform qnx|marathon]',
  '                       [--host-preview] [--window-chrome] [--dry-run]',
  '',
  'QNX defaults to the QNX toolchain; --host-preview is an explicit Linux-only',
  'QNX app preview. Marathon builds a separate Linux executable using Qt/Wayland.',
  '--window-chrome compiles the experimental title bar from native-app.json',
  '"window" (RNQNX_ENABLE_WINDOW_CHROME, default OFF; see docs/adr/0016).',
  'Requires the reviewed source checkout/native prerequisites. No deployment.',
].join('\n');

/** @param {unknown} value */
function isMissing(value) {
  return value instanceof Error && 'code' in value && value.code === 'ENOENT';
}

/** @param {string} file */
async function smallRegular(file) {
  const stat = await lstat(file);
  if (!stat.isFile() || stat.isSymbolicLink() || !stat.size || stat.size > 65536) {
    throw new Error('Expected a nonempty native descriptor under 64 KiB.');
  }
  return readFile(file, 'utf8');
}

/** @param {string} root @param {string} platform @param {boolean} preview @param {boolean} [windowChrome] */
export async function planNativeBuild(
  root,
  platform = 'qnx',
  preview = false,
  windowChrome = false
) {
  if (preview && platform !== 'qnx') throw new Error('--host-preview applies only to QNX.');
  if (preview && process.platform !== 'linux') {
    throw new Error('The QNX Linux preview requires a Linux development host.');
  }
  if (platform === 'marathon' && process.platform !== 'linux') {
    throw new Error('The native Marathon client build currently requires Linux.');
  }
  if (!['qnx', 'marathon'].includes(platform)) throw new Error('Use qnx or marathon.');
  const project = await realpath(path.resolve(root));
  const descriptorPath = path.join(project, 'build', platform, 'native-app.json');
  const original = JSON.parse(await smallRegular(descriptorPath));
  const window = readWindowDescriptor(original);
  if (windowChrome && platform === 'marathon')
    throw new Error('--window-chrome is not supported for Marathon yet (docs/adr/0016).');
  // Revalidate the exact selected bundle, including QNX .hbc inputs. A stale
  // descriptor must never silently point to a different JS entry.
  const relativeBundle = path.relative(project, original.bundle ?? '');
  const { configuration, expoConfig } = await nativeConfiguration(
    project,
    platform,
    relativeBundle
  );
  const { shortName, icon, labelFit, ...base } = original;
  if (JSON.stringify(base) !== JSON.stringify(configuration) || !labelFit) {
    throw new Error(
      'Native descriptor is stale. Review the bundle and run project prepare-native.'
    );
  }
  await verifyPreparedIdentity(
    project,
    /** @type {import('./app-identity.mjs').ExpoConfig} */ (expoConfig.exp),
    { shortName, icon }
  );
  const buildDir = path.join(project, 'build', platform, preview ? 'preview-native' : 'native');
  try {
    const current = await lstat(buildDir);
    if (!current.isDirectory() || current.isSymbolicLink()) {
      throw new Error('Native build directory must not be a symlink or non-directory.');
    }
  } catch (error) {
    if (!isMissing(error)) throw error;
  }
  const preset = platform === 'qnx' && !preview ? 'qnx-aarch64-debug' : 'linux-host-renderer';
  const flags = [
    '-DRNQNX_BUILD_CONSUMER_APP=ON',
    '-DRNQNX_CONSUMER_CONFIG=' + descriptorPath,
    ...(platform === 'qnx' && !preview
      ? [
          '-DRNQNX_BUILD_REACT_QNX=ON',
          '-DRNQNX_BUILD_HERMES_QNX=ON',
          '-DRNQNX_FETCH_REACT_NATIVE_DEPS=ON',
        ]
      : []),
    ...(preview ? ['-DRNQNX_CONSUMER_HOST_PREVIEW=ON'] : []),
    ...(platform === 'marathon' ? ['-DRNQNX_BUILD_MARATHON_HOST=ON'] : []),
    ...windowChromeCMakeFlags(window, windowChrome),
  ];
  if (platform === 'qnx' && !preview && !configuration.bundle.endsWith('.hbc')) {
    throw new Error('Real QNX builds require a prepared matching Hermes .hbc bundle.');
  }
  if (platform === 'qnx' && !preview && (!process.env.QNX_HOST || !process.env.QNX_TARGET)) {
    throw new Error('QNX_HOST and QNX_TARGET must select an installed licensed QNX SDK.');
  }
  const output = path.join(
    buildDir,
    'consumer',
    platform,
    configuration.appId,
    configuration.appId
  );
  return {
    project,
    platform,
    preview,
    appId: configuration.appId,
    window,
    descriptorPath,
    buildDir,
    output,
    configure: ['cmake', '--preset', preset, '-S', platformRoot, '-B', buildDir, ...flags],
    build: ['cmake', '--build', buildDir, '--target', 'rnqnx_consumer_app', '--parallel', '2'],
    nativeBuildVerified: false,
    runtimeVerified: false,
  };
}

/** @param {string[]} command */
async function execute(command) {
  const [executable, ...args] = command;
  await new Promise((resolve, reject) => {
    const child = spawn(executable, args, {
      cwd: platformRoot,
      stdio: 'inherit',
      shell: false,
      env: process.env,
    });
    child.once('error', reject);
    child.once('exit', (code, signal) => {
      if (code === 0) resolve(undefined);
      else reject(new Error(executable + ' failed: ' + (signal ?? code)));
    });
  });
}

/** @param {string[]} args */
export async function buildNativeMain(args) {
  const { values, positionals, tokens } = parseArgs({
    args,
    strict: true,
    allowPositionals: true,
    tokens: true,
    options: {
      help: { type: 'boolean', short: 'h' },
      project: { type: 'string' },
      platform: { type: 'string' },
      'host-preview': { type: 'boolean' },
      'dry-run': { type: 'boolean' },
      'window-chrome': { type: 'boolean' },
    },
  });
  const seen = new Set();
  for (const token of tokens) {
    if (token.kind !== 'option') continue;
    if (seen.has(token.name)) throw new Error('Duplicate option --' + token.name);
    seen.add(token.name);
  }
  if (values.help || args.length === 0) {
    console.log(HELP);
    return 0;
  }
  if (positionals.length) throw new Error('build-native accepts no positional arguments.');
  const plan = await planNativeBuild(
    values.project ?? '.',
    values.platform ?? 'qnx',
    !!values['host-preview'],
    !!values['window-chrome']
  );
  if (values['dry-run']) {
    console.log(JSON.stringify({ ...plan, action: 'dry-run' }, null, 2));
    return 0;
  }
  await mkdir(plan.buildDir, { recursive: true });
  await execute(plan.configure);
  await execute(plan.build);
  const stat = await lstat(plan.output);
  if (!stat.isFile() || stat.isSymbolicLink() || !stat.size || stat.size > 1024 * 1024 * 1024) {
    throw new Error('CMake completed without a bounded regular native executable.');
  }
  const executable = await realpath(plan.output);
  if (!executable.startsWith(plan.buildDir + path.sep)) {
    throw new Error('Native executable escaped the build directory.');
  }
  const hash = createHash('sha256');
  for await (const chunk of createReadStream(executable)) hash.update(chunk);
  const sha256 = hash.digest('hex');
  console.log(
    JSON.stringify(
      {
        appId: plan.appId,
        platform: plan.platform,
        preview: plan.preview,
        executable,
        sha256,
        nativeBuildVerified: true,
        runtimeVerified: false,
      },
      null,
      2
    )
  );
  return 0;
}
