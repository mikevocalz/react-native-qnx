#!/usr/bin/env node
import { createHash } from 'node:crypto';
import { lstat, mkdir, readFile, readdir, realpath, rm, writeFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { parseArgs } from 'node:util';

const require = createRequire(import.meta.url);
const help = `Expo QNX preparation (source checkout)

  inspect --project <directory>
  prepare --project <directory> [--arch aarch64|x86_64]
          [--out build/expo-qnx] [--dry-run | --check]

--project defaults to the current directory. --out is relative to the project
and must be a directory below its build/. prepare creates a NEW directory or
verifies identical existing files; it never overwrites changed output.
--check only verifies; --dry-run discovers modules without writing files.

This generates linking metadata, not a native app, Expo prebuild, a module
registry, or proof of QNX runtime support. No CMake/build/install is executed.
`;

/** @typedef {{packageName: string, packageRoot: string, version: string,
 * cmakeListsPath: string, target: string, modules: string[], binaryName: string}} Descriptor */
/** @param {unknown} value @param {string} label @returns {Record<string, unknown>} */
function object(value, label) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    throw new Error(`${label} must be an object.`);
  }
  return /** @type {Record<string, unknown>} */ (value);
}
/** @param {unknown} value @param {string} label @param {RegExp} pattern */
function text(value, label, pattern) {
  if (typeof value !== 'string' || value.length > 4096 || !pattern.test(value)) {
    throw new Error(`Invalid ${label}.`);
  }
  return value;
}
/** @param {string} root @param {string} candidate */
function inside(root, candidate) {
  const relative = path.relative(root, candidate);
  return (
    relative !== '' &&
    !path.isAbsolute(relative) &&
    relative !== '..' &&
    !relative.startsWith(`..${path.sep}`)
  );
}
/** @param {string} file */
async function optionalStat(file) {
  try {
    return await lstat(file);
  } catch (error) {
    if (error instanceof Error && 'code' in error && error.code === 'ENOENT') return null;
    throw error;
  }
}
/** @param {unknown} input @returns {Promise<Descriptor[]>} */
export async function validateDescriptors(input) {
  if (!Array.isArray(input) || input.length > 128) {
    throw new Error('Discovery must return at most 128 QNX modules.');
  }
  const names = new Set();
  const targets = new Set();
  const binaries = new Set();
  /** @type {Descriptor[]} */
  const result = [];
  for (const item of input) {
    const descriptor = object(item, 'Module descriptor');
    const packageName = text(
      descriptor.packageName,
      'package name',
      /^(?:@[a-z0-9][a-z0-9._-]*\/)?[a-z0-9][a-z0-9._-]*$/
    );
    const target = text(descriptor.target, 'CMake target', /^[A-Za-z_][A-Za-z0-9_.+-]*$/);
    const binaryName = packageName.replace(/[^A-Za-z0-9_.-]/g, '_');
    if (names.has(packageName) || targets.has(target) || binaries.has(binaryName)) {
      throw new Error('Duplicate module package, CMake target or binary directory.');
    }
    names.add(packageName);
    targets.add(target);
    binaries.add(binaryName);
    const root = text(descriptor.packageRoot, 'module root', /\S/);
    if (!path.isAbsolute(root)) throw new Error('Module root must be absolute.');
    const packageRoot = await realpath(root);
    const cmakeListsPath = text(descriptor.cmakeListsPath, 'CMake source path', /\S/);
    if (
      path.isAbsolute(cmakeListsPath) ||
      path.win32.isAbsolute(cmakeListsPath) ||
      cmakeListsPath.includes('\\') ||
      cmakeListsPath.split('/').includes('..') ||
      path.basename(cmakeListsPath) !== 'CMakeLists.txt'
    ) {
      throw new Error('CMake source path must name an in-module CMakeLists.txt.');
    }
    const cmakeFile = path.resolve(packageRoot, cmakeListsPath);
    const canonical = await realpath(cmakeFile);
    const info = await lstat(cmakeFile);
    if (!inside(packageRoot, canonical) || info.isSymbolicLink() || !info.isFile()) {
      throw new Error('CMake source must be a regular file inside its module.');
    }
    // The existing generator quotes paths but does not escape CMake expansion.
    // Reject metacharacters before passing any descriptor to that generator.
    if (/[\x00-\x1f";$]/.test(packageRoot + '/' + cmakeListsPath)) {
      throw new Error('CMake source path contains unsupported metacharacters.');
    }
    if (!Array.isArray(descriptor.modules) || descriptor.modules.length > 256) {
      throw new Error('Module class list must be an array of at most 256 names.');
    }
    const modules = descriptor.modules.map((name) =>
      text(name, 'module class', /^[A-Za-z_][A-Za-z0-9_.:]*$/)
    );
    result.push({
      packageName,
      packageRoot,
      cmakeListsPath,
      target,
      modules,
      binaryName,
      version: text(descriptor.version, 'module version', /^[0-9A-Za-z.+-]{1,128}$/),
    });
  }
  return result.sort((a, b) =>
    a.packageName < b.packageName ? -1 : a.packageName > b.packageName ? 1 : 0
  );
}

/** @param {string} project @param {string} requested @param {boolean} createParents */
async function outputPath(project, requested, createParents) {
  const build = path.join(project, 'build');
  const output = path.resolve(project, requested);
  if (!inside(build, output)) throw new Error('--out must be below the project build/ directory.');
  const parts = path.relative(project, output).split(path.sep);
  let current = project;
  for (let index = 0; index < parts.length; index += 1) {
    current = path.join(current, parts[index]);
    if (createParents && index < parts.length - 1) {
      try {
        await mkdir(current, { mode: 0o700 });
      } catch (error) {
        if (!(error instanceof Error && 'code' in error && error.code === 'EEXIST')) throw error;
      }
    }
    const info = await optionalStat(current);
    if (info && (info.isSymbolicLink() || !info.isDirectory())) {
      throw new Error('Output path must use real directories, not symlinks or files.');
    }
  }
  return output;
}
/** @param {string} output @param {Map<string, string>} files */
async function checkFiles(output, files) {
  const actual = (await readdir(output)).sort();
  if (JSON.stringify(actual) !== JSON.stringify([...files.keys()].sort())) {
    throw new Error('Prepared directory has missing or extra files; choose a new --out.');
  }
  for (const [name, expected] of files) {
    const file = path.join(output, name);
    const info = await lstat(file);
    if (
      !info.isFile() ||
      info.isSymbolicLink() ||
      info.size !== Buffer.byteLength(expected) ||
      (await readFile(file, 'utf8')) !== expected
    ) {
      throw new Error(`Prepared file differs or is unsafe: ${name}; choose a new --out.`);
    }
  }
}

/** @param {string} project @param {string} requested @param {unknown} descriptors
 * @param {{arch?: string, check?: boolean, dryRun?: boolean}} [options] */
export async function prepareProject(project, requested, descriptors, options = {}) {
  if (options.check && options.dryRun) throw new Error('--check and --dry-run cannot be combined.');
  const arch = options.arch ?? 'aarch64';
  if (arch !== 'aarch64' && arch !== 'x86_64') throw new Error('Use --arch aarch64 or x86_64.');
  const projectRoot = await realpath(project);
  const output = await outputPath(projectRoot, requested, false);
  const modules = await validateDescriptors(descriptors);
  const { generateQnxCMake } = require('../../expo-qnx-autolinking/src/index.js');
  const withQnxProject = require('../../expo-qnx-config-plugins/index.js');
  const relative = path.relative(projectRoot, output).split(path.sep).join('/');
  const config = withQnxProject(
    {},
    {
      targetArch: arch,
      autolinkingJson: `${relative}/modules.json`,
      autolinkingCMake: `${relative}/modules.cmake`,
    }
  );
  const files = new Map([
    [
      'modules.json',
      JSON.stringify({ schemaVersion: 1, platform: 'qnx', modules }, null, 2) + '\n',
    ],
    ['modules.cmake', generateQnxCMake(modules)],
    ['config.json', JSON.stringify(config, null, 2) + '\n'],
  ]);
  const manifest = {
    schemaVersion: 1,
    platform: 'qnx',
    targetArch: arch,
    moduleCount: modules.length,
    nativeBuildVerified: false,
    runtimeVerified: false,
    files: [...files].map(([name, contents]) => ({
      name,
      bytes: Buffer.byteLength(contents),
      sha256: createHash('sha256').update(contents).digest('hex'),
    })),
  };
  files.set('manifest.json', JSON.stringify(manifest, null, 2) + '\n');
  if (options.dryRun) return { action: 'dry-run', output, ...manifest };
  if (await optionalStat(output)) {
    await checkFiles(output, files);
    return { action: 'unchanged', output, ...manifest };
  }
  if (options.check) throw new Error('Prepared directory is missing; run prepare without --check.');
  await outputPath(projectRoot, requested, true);
  await mkdir(output, { mode: 0o700 });
  try {
    // manifest.json is last. A partial directory never verifies as complete.
    for (const [name, contents] of files) {
      await writeFile(path.join(output, name), contents, { flag: 'wx', mode: 0o600 });
    }
  } catch (error) {
    await rm(output, { recursive: true, force: true });
    throw error;
  }
  return { action: 'created', output, ...manifest };
}

/** @param {string[]} argv @param {string} [cwd] */
export async function main(argv, cwd = process.cwd()) {
  const { values, positionals, tokens } = parseArgs({
    args: argv,
    allowPositionals: true,
    strict: true,
    tokens: true,
    options: {
      help: { type: 'boolean', short: 'h' },
      project: { type: 'string' },
      out: { type: 'string' },
      arch: { type: 'string' },
      'dry-run': { type: 'boolean' },
      check: { type: 'boolean' },
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
  if (positionals.length !== 1 || !['inspect', 'prepare'].includes(command)) {
    throw new Error('Use inspect or prepare; run --help for the supported scope.');
  }
  if (command === 'inspect' && Object.keys(values).some((key) => key !== 'project')) {
    throw new Error('inspect accepts only --project.');
  }
  if (
    (values.project !== undefined && !values.project.trim()) ||
    (values.out !== undefined && !values.out.trim())
  )
    throw new Error('Paths must not be empty.');
  if (values.check && values['dry-run'])
    throw new Error('--check and --dry-run cannot be combined.');
  if (values.arch && !['aarch64', 'x86_64'].includes(values.arch)) {
    throw new Error('Use --arch aarch64 or x86_64.');
  }
  const project = await realpath(path.resolve(cwd, values.project ?? '.'));
  const pkg = JSON.parse(await readFile(path.join(project, 'package.json'), 'utf8'));
  if (!pkg.name) throw new Error('Project package.json must declare its name.');
  const { findQnxExpoModules } = require('../../expo-qnx-autolinking/src/index.js');
  const modules = await findQnxExpoModules(project);
  const report =
    command === 'inspect'
      ? { platform: 'qnx', modules: await validateDescriptors(modules), runtimeVerified: false }
      : await prepareProject(project, values.out ?? 'build/expo-qnx', modules, {
          arch: values.arch,
          check: values.check,
          dryRun: values['dry-run'],
        });
  console.log(JSON.stringify(report, null, 2));
  return 0;
}
if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    process.exitCode = await main(process.argv.slice(2));
  } catch (error) {
    console.error(`expo-qnx: ${error instanceof Error ? error.message : error}`);
    process.exitCode = 1;
  }
}
