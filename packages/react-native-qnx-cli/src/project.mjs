#!/usr/bin/env node
import { spawn } from 'node:child_process';
import { lstat, readFile, realpath, mkdir, mkdtemp, link, chmod, rm } from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';
import { parseArgs } from 'node:util';

const help = `React Native QNX external-project bundling

  project inspect --project <directory> [--platform qnx|marathon] [--entry index.js]
  project bundle  --project <directory> [--platform qnx|marathon] [--entry index.js]
                  [--out build/<platform>/app.bundle.js] [--dry-run]

QNX is the default. The project supplies installed Metro and its target config:
metro.qnx.config.js (else metro.config.js) for qnx; metro.marathon.config.js for marathon.
Output must be a NEW file below the project's build/ directory.
This produces JavaScript only, not a native project, module registration or deployment.
`;

/** @param {unknown} error */
function missing(error) {
  return error instanceof Error && 'code' in error && error.code === 'ENOENT';
}
/** @param {string} parent @param {string} child */
function inside(parent, child) {
  const relative = path.relative(parent, child);
  return (
    relative !== '' &&
    relative !== '..' &&
    !relative.startsWith(`..${path.sep}`) &&
    !path.isAbsolute(relative)
  );
}
/** @param {string} root @param {string} relative @param {boolean} [file] */
async function confined(root, relative, file = false) {
  if (
    !relative ||
    path.isAbsolute(relative) ||
    path.win32.isAbsolute(relative) ||
    /[\x00-\x1f\\]/.test(relative)
  ) {
    throw new Error('Use a nonempty project-relative path without control characters.');
  }
  if (relative.split('/').some((part) => !part || part === '.' || part === '..')) {
    throw new Error('Paths must not contain empty, dot or parent segments.');
  }
  const target = path.resolve(root, relative);
  if (!inside(root, target)) throw new Error('Path must stay inside the project.');
  let current = root;
  const parts = path.relative(root, target).split(path.sep);
  for (const [index, part] of parts.entries()) {
    current = path.join(current, part);
    try {
      const stat = await lstat(current);
      if (stat.isSymbolicLink())
        throw new Error('Project inputs and output paths must not contain symlinks.');
      if (index < parts.length - 1 && !stat.isDirectory())
        throw new Error('Path parent must be a directory.');
      if (file && index === parts.length - 1 && !stat.isFile())
        throw new Error('Input must be a regular file.');
    } catch (error) {
      if (file || !missing(error)) throw error;
    }
  }
  return target;
}

/** Plan from caller-owned project files; never evaluates the Metro config or writes.
 * @param {string[]} argv @param {string} [cwd] */
export async function planProject(argv, cwd = process.cwd()) {
  const { values, positionals, tokens } = parseArgs({
    args: argv,
    strict: true,
    allowPositionals: true,
    tokens: true,
    options: {
      help: { type: 'boolean', short: 'h' },
      project: { type: 'string' },
      platform: { type: 'string' },
      entry: { type: 'string' },
      out: { type: 'string' },
      'dry-run': { type: 'boolean' },
    },
  });
  const seen = new Set();
  for (const token of tokens) {
    if (token.kind !== 'option') continue;
    if (seen.has(token.name)) throw new Error(`Duplicate option --${token.name}.`);
    seen.add(token.name);
  }
  if (values.help || argv.length === 0) return null;
  const command = positionals[0];
  if (positionals.length !== 1 || !['inspect', 'bundle'].includes(command)) {
    throw new Error('Use project inspect or project bundle.');
  }
  if (command === 'inspect' && (values.out !== undefined || values['dry-run'])) {
    throw new Error('inspect does not accept --out or --dry-run.');
  }
  const platform = values.platform ?? 'qnx';
  if (!['qnx', 'marathon'].includes(platform)) throw new Error('Use --platform qnx or marathon.');
  if (values.project !== undefined && !values.project.trim())
    throw new Error('--project must not be empty.');
  const root = await realpath(path.resolve(cwd, values.project ?? '.'));
  const manifestPath = await confined(root, 'package.json', true);
  if ((await lstat(manifestPath)).size > 1024 * 1024)
    throw new Error('Project manifest exceeds 1 MiB.');
  const manifest = JSON.parse(await readFile(manifestPath, 'utf8'));
  if (!manifest || typeof manifest.name !== 'string' || !manifest.name.trim()) {
    throw new Error('Project package.json must have a name.');
  }
  const entry = await confined(root, values.entry ?? 'index.js', true);
  // An app that added QNX with project add-qnx keeps metro.config.js for
  // iOS/Android/web and carries its QNX config in metro.qnx.config.js.
  let qnxConfigName = 'metro.config.js';
  if (platform === 'qnx') {
    try {
      await lstat(path.join(root, 'metro.qnx.config.js'));
      qnxConfigName = 'metro.qnx.config.js';
    } catch (error) {
      if (!missing(error)) throw error;
    }
  }
  const config = await confined(
    root,
    platform === 'qnx' ? qnxConfigName : 'metro.marathon.config.js',
    true
  );
  const relativeOutput = values.out ?? `build/${platform}/app.bundle.js`;
  const output = await confined(root, relativeOutput);
  if (!inside(path.join(root, 'build'), output) || !output.endsWith('.js')) {
    throw new Error('--out must name a new .js file below project build/.');
  }
  if (command === 'bundle') {
    try {
      await lstat(output);
      throw new Error('Bundle output already exists; choose another --out.');
    } catch (error) {
      if (!missing(error)) throw error;
    }
  }
  const require = createRequire(manifestPath);
  const metroManifestPath = require.resolve('metro/package.json');
  const metro = JSON.parse(await readFile(metroManifestPath, 'utf8'));
  const bin = typeof metro.bin === 'string' ? metro.bin : metro.bin?.metro;
  if (typeof bin !== 'string' || !bin || path.isAbsolute(bin))
    throw new Error('Installed Metro has no relative CLI entry.');
  const metroRoot = await realpath(path.dirname(metroManifestPath));
  const metroCli = await realpath(path.resolve(metroRoot, bin));
  if (!inside(metroRoot, metroCli) || !(await lstat(metroCli)).isFile()) {
    throw new Error('Installed Metro CLI must be a regular file inside its package.');
  }
  return {
    command,
    platform,
    project: root,
    projectName: manifest.name,
    entry,
    config,
    output,
    executable: process.execPath,
    metroCli,
    metroVersion: String(metro.version),
    dryRun: !!values['dry-run'],
    nativeBuildVerified: false,
    runtimeVerified: false,
  };
}

/** @param {string} executable @param {string[]} args @param {string} cwd */
async function runMetro(executable, args, cwd) {
  if (!['linux', 'darwin'].includes(process.platform)) {
    throw new Error('This experimental bundle supervisor supports Linux and macOS hosts only.');
  }
  return new Promise((/** @type {(code: number) => void} */ resolve, reject) => {
    const child = spawn(executable, args, { cwd, shell: false, stdio: 'inherit', detached: true });
    let forcedCode = 0;
    /** @type {ReturnType<typeof setTimeout> | undefined} */
    let killTimer;
    /** @param {NodeJS.Signals} signal */
    const signalGroup = (signal) => {
      if (!child.pid) return;
      try {
        process.kill(-child.pid, signal);
      } catch (error) {
        if (!(error instanceof Error && 'code' in error && error.code === 'ESRCH')) throw error;
      }
    };
    /** @param {number} code @param {NodeJS.Signals} signal */
    const stop = (code, signal) => {
      if (forcedCode) return;
      forcedCode = code;
      signalGroup(signal);
      killTimer = setTimeout(() => signalGroup('SIGKILL'), 1000);
    };
    const interrupt = () => stop(130, 'SIGINT');
    const terminate = () => stop(143, 'SIGTERM');
    const timeout = setTimeout(() => stop(124, 'SIGTERM'), 120000);
    const cleanup = () => {
      clearTimeout(timeout);
      clearTimeout(killTimer);
      process.off('SIGINT', interrupt);
      process.off('SIGTERM', terminate);
      // Metro may have worker processes; do not leave this owned group running.
      signalGroup('SIGKILL');
    };
    process.on('SIGINT', interrupt);
    process.on('SIGTERM', terminate);
    child.once('error', (error) => {
      cleanup();
      reject(error);
    });
    child.once('close', (code) => {
      cleanup();
      resolve(forcedCode || (code ?? 1));
    });
  });
}

/** @param {string[]} argv @param {string} [cwd] */
export async function projectMain(argv, cwd = process.cwd()) {
  const plan = await planProject(argv, cwd);
  if (!plan) {
    console.log(help);
    return 0;
  }
  if (plan.command === 'inspect' || plan.dryRun) {
    console.log(JSON.stringify(plan, null, 2));
    return 0;
  }
  // All project/dependency/config paths are trusted developer inputs. These
  // checks reject existing escapes; they are not a concurrent-filesystem sandbox.
  await confined(plan.project, path.relative(plan.project, plan.output).split(path.sep).join('/'));
  await mkdir(path.dirname(plan.output), { recursive: true });
  const staging = await mkdtemp(path.join(path.dirname(plan.output), '.rnqnx-bundle-'));
  const stagedOutput = path.join(staging, 'bundle.js');
  try {
    const code = await runMetro(
      plan.executable,
      [
        plan.metroCli,
        'build',
        plan.entry,
        '--config',
        plan.config,
        '--platform',
        plan.platform,
        '--dev',
        'false',
        '--max-workers',
        '2',
        '--out',
        stagedOutput,
      ],
      plan.project
    );
    if (code !== 0) return code;
    const stat = await lstat(stagedOutput);
    if (stat.isSymbolicLink() || !stat.isFile() || stat.size === 0) {
      throw new Error('Metro did not produce a nonempty regular bundle.');
    }
    await chmod(stagedOutput, 0o600);
    // Same-directory hard link publishes atomically and refuses any existing
    // destination, including a symlink created while Metro was running.
    await link(stagedOutput, plan.output);
    console.log(JSON.stringify({ ...plan, action: 'bundled', bytes: stat.size }, null, 2));
    return 0;
  } finally {
    await rm(staging, { recursive: true, force: true });
  }
}
