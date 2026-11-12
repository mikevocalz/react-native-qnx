#!/usr/bin/env node
import { spawn } from 'node:child_process';
import { constants } from 'node:os';
import {
  access,
  mkdir,
  readFile,
  writeFile,
  rm,
  realpath,
  lstat,
  readlink,
} from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';
import { parseArgs } from 'node:util';
import { fileURLToPath } from 'node:url';

const help = `React Native QNX (source checkout)

Usage: pnpm qnx <command> [options]

  init <name>       Create apps/<name>; never overwrites an existing app
  bundle-qnx       Production Metro bundle for --app <name> [--out build/<file>]
  doctor           Check host tools, the QNX SDP environment and target settings;
                   each failure names its fix
  configure-qnx    Configure the existing QNX CMake preset
  build-qnx        Build the existing QNX CMake preset
  deploy-qnx       Deploy one executable with the existing atomic deploy script
  run-qnx          Run one deployed executable
  log-qnx          Stream target slog2 output
  evidence verify  Check captured Pi evidence and write its SHA-256 manifest

Build/deploy: --arch aarch64|x86_64 (default aarch64)
Build/configure/deploy: --config Debug|Release (default Debug)
Deploy/run: --target <CMake-target> (default rnqnx_playground, the Screen probe)
Evidence: --dir <path> (default: latest artifacts/pi directory)
All commands: --help; executable commands: --dry-run (print argv without executing)

bundle-qnx creates JavaScript only. The native runner requires the registered
surface name RNQnxPlayground. rnqnx_playground_qnx is the React Native runtime;
its Qt/Hermes build and deployment prerequisites are separate from the Screen
probe presets. No npm publication, Qt provisioning or boot modification occurs.
`;

/** @param {string} from */
export async function findWorkspace(from) {
  let candidate = path.resolve(from);
  for (;;) {
    try {
      const pkg = JSON.parse(await readFile(path.join(candidate, 'package.json'), 'utf8'));
      if (pkg.name === 'react-native-qnx-monorepo') {
        await access(path.join(candidate, 'CMakePresets.json'));
        return candidate;
      }
    } catch (error) {
      if (!(error instanceof Error && 'code' in error && error.code === 'ENOENT')) throw error;
    }
    const parent = path.dirname(candidate);
    if (parent === candidate)
      throw new Error(
        'Run this command inside a react-native-qnx clone. Clone it with git clone https://github.com/mikevocalz/react-native-qnx.git, cd into it, then run pnpm install --frozen-lockfile.'
      );
    candidate = parent;
  }
}

/** @param {string | undefined} name */
function appName(name) {
  if (!name || !/^[a-z][a-z0-9-]{0,62}$/.test(name)) {
    throw new Error(
      'App name must be 1–63 lowercase letters, digits or hyphens, starting with a letter.'
    );
  }
  return name;
}

/** @param {string} root @param {string} name */
async function init(root, name) {
  const destination = path.join(root, 'apps', appName(name));
  const workspace = JSON.parse(await readFile(path.join(root, 'package.json'), 'utf8'));
  const manifest = {
    name: `@rn-qnx/${name}`,
    version: '0.1.0',
    private: true,
    main: 'index.js',
    dependencies: {
      react: workspace.devDependencies.react,
      'react-native': workspace.devDependencies['react-native'],
    },
    devDependencies: { '@rn-qnx/metro-config-qnx': 'workspace:*' },
  };
  const files = new Map([['package.json', JSON.stringify(manifest, null, 2) + '\n']]);
  for (const file of ['index.js', 'metro.config.js', 'babel.config.js', 'README.md']) {
    const source = await readFile(path.join(root, 'template/qnx', file), 'utf8');
    files.set(file, source.replaceAll('__APP_NAME__', name));
  }
  try {
    await mkdir(destination);
  } catch (error) {
    if (error instanceof Error && 'code' in error && error.code === 'EEXIST') {
      throw new Error(`apps/${name} already exists; choose another name.`);
    }
    throw error;
  }
  try {
    for (const [file, contents] of files) {
      await writeFile(path.join(destination, file), contents, { flag: 'wx' });
    }
  } catch (error) {
    await rm(destination, { recursive: true, force: true });
    throw error;
  }
  console.log(
    `Created apps/${name}.\nRun pnpm install --no-frozen-lockfile, then:\npnpm qnx bundle-qnx --app ${name}`
  );
}

/** @param {string} executable @param {string[]} args @param {string} cwd */
async function execute(executable, args, cwd) {
  return new Promise((/** @type {(code: number) => void} */ resolve, reject) => {
    const child = spawn(executable, args, { cwd, stdio: 'inherit', shell: false });
    const interrupt = () => child.kill('SIGINT');
    const terminate = () => child.kill('SIGTERM');
    const cleanup = () => {
      process.off('SIGINT', interrupt);
      process.off('SIGTERM', terminate);
    };
    process.on('SIGINT', interrupt);
    process.on('SIGTERM', terminate);
    child.once('error', (error) => {
      cleanup();
      reject(error);
    });
    child.once('exit', (code, signal) => {
      cleanup();
      resolve(code ?? (signal ? 128 + constants.signals[signal] : 1));
    });
  });
}

/** Resolve symlinks even when the final output directories do not exist yet.
 * @param {string} file
 * @param {Set<string>} [links]
 * @returns {Promise<string>}
 */
async function resolveOutputPath(file, links = new Set()) {
  try {
    return await realpath(file);
  } catch (error) {
    if (!(error instanceof Error && 'code' in error && error.code === 'ENOENT')) throw error;
    let stat;
    try {
      stat = await lstat(file);
    } catch (statError) {
      if (!(statError instanceof Error && 'code' in statError && statError.code === 'ENOENT'))
        throw statError;
    }
    if (stat?.isSymbolicLink()) {
      if (links.has(file)) throw new Error('Bundle output contains a symlink cycle.');
      links.add(file);
      return resolveOutputPath(path.resolve(path.dirname(file), await readlink(file)), links);
    }
    const parent = path.dirname(file);
    if (parent === file) throw error;
    return path.join(await resolveOutputPath(parent, links), path.basename(file));
  }
}

/** @param {string} parent @param {string} child */
function isInside(parent, child) {
  const relative = path.relative(parent, child);
  return (
    relative !== '' &&
    relative !== '..' &&
    !relative.startsWith(`..${path.sep}`) &&
    !path.isAbsolute(relative)
  );
}

/** @param {string[]} argv @param {string} [cwd] */
export async function main(argv, cwd = process.cwd()) {
  const { values, positionals } = parseArgs({
    args: argv,
    strict: true,
    allowPositionals: true,
    options: {
      help: { type: 'boolean', short: 'h' },
      'dry-run': { type: 'boolean' },
      app: { type: 'string' },
      out: { type: 'string' },
      arch: { type: 'string' },
      config: { type: 'string' },
      target: { type: 'string' },
      dir: { type: 'string' },
    },
  });
  if (values.help || argv.length === 0) {
    console.log(help);
    return 0;
  }
  const [command, ...rest] = positionals;
  /** @type {Record<string, string[]>} */
  const allowed = {
    init: [],
    'bundle-qnx': ['app', 'out', 'dry-run'],
    doctor: ['dry-run'],
    'configure-qnx': ['arch', 'config', 'dry-run'],
    'build-qnx': ['arch', 'config', 'dry-run'],
    'deploy-qnx': ['arch', 'config', 'target', 'dry-run'],
    'run-qnx': ['arch', 'target', 'dry-run'],
    'log-qnx': ['dry-run'],
    evidence: ['dir', 'dry-run'],
  };
  if (command === undefined)
    throw new Error('Missing command. Run pnpm qnx --help to list commands.');
  if (!Object.hasOwn(allowed, command))
    throw new Error(`Unknown command: ${command}. Run pnpm qnx --help to list commands.`);
  for (const option of Object.keys(values)) {
    if (!allowed[command].includes(option))
      throw new Error(`${command} does not accept --${option}.`);
  }
  if (command === 'init' && rest.length !== 1)
    throw new Error('init needs exactly one app name, for example: pnpm qnx init dashboard');
  if (command === 'evidence' && rest.length !== 1)
    throw new Error('Use pnpm qnx evidence verify [--dir <path>].');
  if (command !== 'init' && command !== 'evidence' && rest.length)
    throw new Error(
      `${command} takes options only; remove ${rest.join(' ')}. Run pnpm qnx --help for its options.`
    );
  if (command === 'evidence' && rest[0] !== 'verify')
    throw new Error('Use pnpm qnx evidence verify [--dir <path>].');
  const root = await findWorkspace(cwd);
  if (command === 'init') {
    await init(root, rest[0]);
    return 0;
  }
  const arch = values.arch ?? 'aarch64';
  const config = values.config ?? 'Debug';
  const target = values.target ?? 'rnqnx_playground';
  if (!['aarch64', 'x86_64'].includes(arch)) throw new Error('Use --arch aarch64 or x86_64.');
  if (!['Debug', 'Release'].includes(config)) throw new Error('Use --config Debug or Release.');
  if (!/^[A-Za-z0-9][A-Za-z0-9._-]*$/.test(target))
    throw new Error('Target must be a CMake target name, without shell syntax.');
  let executable = 'bash';
  let args = [path.join(root, 'scripts', `${command === 'doctor' ? 'doctor' : command}.sh`)];
  let workingDirectory = root;
  if (command === 'evidence') {
    executable = process.execPath;
    args = [path.join(root, 'scripts/verify-pi-evidence.mjs')];
    if (values.dir !== undefined) {
      if (!values.dir.trim()) throw new Error('--dir must be a non-empty path.');
      args.push(path.resolve(cwd, values.dir));
    }
  } else if (command === 'bundle-qnx') {
    const name = appName(values.app);
    workingDirectory = path.join(root, 'apps', name);
    try {
      await access(path.join(workingDirectory, 'index.js'));
    } catch {
      throw new Error(
        `apps/${name}/index.js not found. Create the app with pnpm qnx init ${name} and run pnpm install --no-frozen-lockfile, or pass an existing --app name.`
      );
    }
    const output = path.resolve(root, values.out ?? `build/${name}.bundle.js`);
    if (!output.startsWith(path.join(root, 'build') + path.sep))
      throw new Error('Bundle output must be inside this checkout’s build/ directory.');
    const realRoot = await realpath(root);
    const buildAnchor = path.join(realRoot, 'build');
    let buildStat;
    try {
      buildStat = await lstat(buildAnchor);
    } catch (error) {
      if (!(error instanceof Error && 'code' in error && error.code === 'ENOENT')) throw error;
    }
    if (buildStat?.isSymbolicLink())
      throw new Error(
        'Bundle output symlinks must stay inside build/: build/ itself must not be a symlink.'
      );
    const realBuild = await resolveOutputPath(buildAnchor);
    const realOutput = await resolveOutputPath(output);
    // Metro opens the output later; this rejects existing escapes, but cannot
    // prevent another process from replacing a checked path before that open.
    if (!isInside(realRoot, realBuild) || !isInside(realBuild, realOutput))
      throw new Error('Bundle output symlinks must stay inside this checkout’s build/ directory.');
    const require = createRequire(path.join(root, 'package.json'));
    let metroPackage;
    try {
      metroPackage = require.resolve('metro/package.json');
    } catch {
      throw new Error(
        'Metro is not installed in this checkout. Run pnpm install --frozen-lockfile from the repository root.'
      );
    }
    const metro = JSON.parse(await readFile(metroPackage, 'utf8'));
    const metroBin = typeof metro.bin === 'string' ? metro.bin : metro.bin?.metro;
    if (typeof metroBin !== 'string') throw new Error('Installed Metro has no CLI entry point.');
    executable = process.execPath;
    args = [
      path.resolve(path.dirname(metroPackage), metroBin),
      'build',
      'index.js',
      '--config',
      path.join(workingDirectory, 'metro.config.js'),
      '--platform',
      'qnx',
      '--dev',
      'false',
      '--max-workers',
      '2',
      '--out',
      output,
    ];
    if (!values['dry-run']) await mkdir(path.dirname(output), { recursive: true });
  } else if (['configure-qnx', 'build-qnx'].includes(command)) args.push(arch, config);
  else if (command === 'deploy-qnx') args.push(arch, target, config);
  else if (command === 'run-qnx') args.push(arch, target);
  if (values['dry-run']) {
    console.log(JSON.stringify({ executable, args, cwd: workingDirectory }, null, 2));
    return 0;
  }
  const code = await execute(executable, args, workingDirectory);
  if (code !== 0) {
    const next = {
      'configure-qnx': 'Run pnpm qnx doctor; it names the missing QNX SDP or build tool.',
      'build-qnx': `Run pnpm qnx configure-qnx --arch ${arch} --config ${config} first; if it fails too, run pnpm qnx doctor.`,
      'deploy-qnx': `Check that pnpm qnx build-qnx --arch ${arch} --config ${config} succeeded and that QNX_TARGET_IP and QNX_TARGET_USER reach the board over SSH.`,
      'run-qnx': `Deploy first with pnpm qnx deploy-qnx --arch ${arch} --target ${target}, and check QNX_TARGET_IP and QNX_TARGET_USER.`,
      'log-qnx': 'Check QNX_TARGET_IP and QNX_TARGET_USER; the board must accept SSH.',
      doctor: '',
      'bundle-qnx':
        'Read the Metro error above; a missing module usually means pnpm install --no-frozen-lockfile was not run after init.',
      evidence: '',
    }[command];
    if (next) console.error(`rnqnx: ${command} exited with code ${code}. ${next}`);
  }
  return code;
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    process.exitCode = await main(process.argv.slice(2));
  } catch (error) {
    console.error(`rnqnx: ${error instanceof Error ? error.message : error}`);
    process.exitCode = 1;
  }
}
