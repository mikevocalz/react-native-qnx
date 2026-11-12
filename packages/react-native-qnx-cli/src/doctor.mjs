#!/usr/bin/env node
import { lstat, open, realpath } from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';
import { parseArgs } from 'node:util';
import { fileURLToPath } from 'node:url';
import {
  identityToolPath,
  measureTileLabel,
  resolveTileLabel,
  validateIconSource,
} from './app-identity.mjs';
import { CONSUMER_CONTRACT } from './consumer-contract.mjs';
import { EXPO_CONFIG_FILES } from './expo-config.mjs';
import { discoverNitroModules } from './nitro-modules.mjs';

const help = `React Native QNX consumer project diagnostics
  rnqnx project doctor --project <directory> [--platform qnx|marathon]
                       [--entry index.js] [--identity-tool <path>] [--json]

Read-only metadata checks. QNX is default. No config/package JavaScript,
package-manager process, compiler, network request or device command runs,
with one opt-in exception: --identity-tool runs that Qt host binary (built
from native/app-identity) to measure the tile label. Without it, the app icon
header and tile label text are checked from static app.json only.
Exit 0: inspected metadata matches; 1: failed check; 2: invalid invocation.
A compatible report is not a native build, security audit or runtime proof.
`;

/** @typedef {{id: string, status: 'passed'|'failed'|'not-checked',
 * message: string, expected?: string, observed?: string}} Check */
/** @param {unknown} value @param {string} label @returns {Record<string, unknown>} */
function object(value, label) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    throw new Error(`${label} must be a JSON object.`);
  }
  return /** @type {Record<string, unknown>} */ (value);
}
/** @param {unknown} error */
function description(error) {
  return error instanceof Error ? error.message : String(error);
}
/** Read at most one MiB; never evaluate the file.
 * @param {string} file */
async function readMetadata(file) {
  const before = await lstat(file);
  if (!before.isFile() || before.isSymbolicLink() || before.size > 1024 * 1024) {
    throw new Error('Metadata must be a regular JSON file of at most 1 MiB.');
  }
  const handle = await open(file, 'r');
  try {
    const info = await handle.stat();
    if (!info.isFile() || info.size > 1024 * 1024)
      throw new Error('Metadata is not a bounded regular file.');
    const bytes = Buffer.alloc(1024 * 1024 + 1);
    let length = 0;
    while (length < bytes.length) {
      const result = await handle.read(bytes, length, bytes.length - length, length);
      if (!result.bytesRead) break;
      length += result.bytesRead;
    }
    if (length > 1024 * 1024) throw new Error('Metadata exceeds 1 MiB.');
    return object(JSON.parse(bytes.subarray(0, length).toString('utf8')), 'Metadata');
  } finally {
    await handle.close();
  }
}
/** @param {string} root @param {string} relative */
async function projectFile(root, relative) {
  if (
    !relative ||
    path.isAbsolute(relative) ||
    path.win32.isAbsolute(relative) ||
    /[\x00-\x1f\\]/.test(relative) ||
    relative.split('/').some((s) => !s || s === '.' || s === '..')
  ) {
    throw new Error('Use a project-relative input without dot segments or control characters.');
  }
  let file = root;
  const parts = relative.split('/');
  for (let index = 0; index < parts.length; index += 1) {
    file = path.join(file, parts[index]);
    const info = await lstat(file);
    if (
      info.isSymbolicLink() ||
      (index < parts.length - 1 ? !info.isDirectory() : !info.isFile())
    ) {
      throw new Error('Project input must be a regular file without symlink paths.');
    }
  }
  return file;
}

/** The identity fields as @expo/config would resolve them for a static
 * app.json: extra.rnqnx, with the @rn-qnx/expo-qnx-config-plugins entry's
 * options merged over it (the plugin's own rule). Plugin options are JSON in
 * app.json, so no plugin or config code runs here.
 * @param {Record<string, unknown>} expo */
export function staticIdentityConfig(expo) {
  const extra = /** @type {Record<string, unknown>} */ (expo.extra ?? {});
  const rnqnx = { .../** @type {Record<string, unknown>} */ (extra.rnqnx ?? {}) };
  for (const entry of Array.isArray(expo.plugins) ? expo.plugins : []) {
    if (!Array.isArray(entry) || typeof entry[0] !== 'string') continue;
    if (!/(^|\/)expo-qnx-config-plugins(\/app\.plugin(\.js)?)?$/.test(entry[0])) continue;
    const options = entry[1];
    if (!options || typeof options !== 'object' || Array.isArray(options)) continue;
    for (const key of ['icon', 'shortName'])
      if (/** @type {Record<string, unknown>} */ (options)[key] !== undefined)
        rnqnx[key] = /** @type {Record<string, unknown>} */ (options)[key];
  }
  return /** @type {import('./app-identity.mjs').ExpoConfig} */ ({
    ...expo,
    extra: { ...extra, rnqnx },
  });
}

/** Check the app icon and tile label from static app.json. app.config.* is
 * code, so its presence leaves these checks to prepare-native.
 * @param {string} root @param {string} platform @param {string | undefined} tool
 * @returns {Promise<Check[]>} */
async function identityChecks(root, platform, tool) {
  for (const name of EXPO_CONFIG_FILES.filter((file) => file !== 'app.json')) {
    try {
      await lstat(path.join(root, name));
    } catch {
      continue;
    }
    return [
      {
        id: 'app-identity',
        status: 'not-checked',
        message: `${name} is code and doctor does not run it; project prepare-native resolves it and checks the icon and tile label.`,
      },
    ];
  }
  let app;
  try {
    app = await readMetadata(await projectFile(root, 'app.json'));
  } catch (error) {
    return [
      {
        id: 'app-identity',
        status: 'not-checked',
        message: `app.json is not readable (${description(error)}); icon and tile label were not checked.`,
      },
    ];
  }
  /** @type {Check[]} */
  const checks = [];
  const expo = staticIdentityConfig(
    /** @type {Record<string, unknown>} */ (
      app.expo && typeof app.expo === 'object' && !Array.isArray(app.expo) ? app.expo : {}
    )
  );
  try {
    const icon = await validateIconSource(root, expo);
    checks.push({
      id: 'app-icon',
      status: 'passed',
      observed: `${icon.width}x${icon.height} PNG`,
      message: `${icon.field} is a square PNG of at least 1024x1024.`,
    });
  } catch (error) {
    checks.push({ id: 'app-icon', status: 'failed', message: description(error) });
  }
  try {
    const tile = resolveTileLabel(expo);
    if (!tool) {
      checks.push({
        id: 'tile-label',
        status: 'not-checked',
        observed: tile.label,
        message: `Tile label comes from ${tile.field}; its rendered width was not measured. Pass --identity-tool to measure it.`,
      });
    } else {
      const fit = measureTileLabel(() => identityToolPath(tool), platform, tile);
      checks.push(
        fit.status === 'measured'
          ? {
              id: 'tile-label',
              status: 'passed',
              observed: `${fit.widthPx.toFixed(1)} px`,
              expected: `<= ${fit.boxWidthPx} px, ${fit.lineCount} line`,
              message: `Tile label from ${tile.field} fits ${fit.launcher} on one line (host measurement, not a device run).`,
            }
          : { id: 'tile-label', status: 'not-checked', message: fit.reason }
      );
    }
  } catch (error) {
    checks.push({ id: 'tile-label', status: 'failed', message: description(error) });
  }
  return checks;
}

/** Inspect only declared files and installed package metadata; never import config code.
 * @param {string} project
 * @param {{platform?: string, entry?: string, identityTool?: string}} [options] */
export async function inspectConsumer(project, options = {}) {
  const platform = options.platform ?? 'qnx';
  if (!['qnx', 'marathon'].includes(platform)) throw new Error('Use --platform qnx or marathon.');
  if (!project.trim()) throw new Error('--project must not be empty.');
  const root = await realpath(path.resolve(project));
  if (!(await lstat(root)).isDirectory()) throw new Error('Project root must be a directory.');
  /** @type {Check[]} */
  const checks = [];
  const nodeOk = Number(process.versions.node.split('.')[0]) >= CONSUMER_CONTRACT.minimumNodeMajor;
  checks.push({
    id: 'node',
    status: nodeOk ? 'passed' : 'failed',
    expected: `>=${CONSUMER_CONTRACT.minimumNodeMajor}`,
    observed: process.versions.node,
    message: nodeOk
      ? 'Node meets the platform minimum.'
      : 'Use the documented Node version before building.',
  });
  let validManifest = false;
  try {
    const manifest = await readMetadata(await projectFile(root, 'package.json'));
    if (typeof manifest.name !== 'string' || !manifest.name.trim())
      throw new Error('Project package.json must declare a name.');
    for (const field of ['dependencies', 'devDependencies']) {
      if (manifest[field] !== undefined) {
        const dependencies = object(manifest[field], field);
        if (Object.values(dependencies).some((value) => typeof value !== 'string')) {
          throw new Error(`${field} must map package names to version strings.`);
        }
      }
    }
    validManifest = true;
    checks.push({
      id: 'project-manifest',
      status: 'passed',
      message: 'Project package metadata is readable and shaped correctly.',
    });
    checks.push(
      manifest.packageManager === undefined
        ? {
            id: 'package-manager',
            status: 'not-checked',
            message:
              'No packageManager declaration; installed package-manager version was not probed.',
          }
        : {
            id: 'package-manager',
            status:
              manifest.packageManager === CONSUMER_CONTRACT.packageManager ? 'passed' : 'failed',
            expected: CONSUMER_CONTRACT.packageManager,
            observed: String(manifest.packageManager),
            message:
              'Only the declared packageManager string was checked; no package-manager process ran.',
          }
    );
  } catch (error) {
    checks.push({ id: 'project-manifest', status: 'failed', message: description(error) });
  }
  for (const [id, relative] of [
    ['entry', options.entry ?? 'index.js'],
    ['metro-config', platform === 'qnx' ? 'metro.config.js' : 'metro.marathon.config.js'],
  ]) {
    try {
      await projectFile(root, relative);
      checks.push({
        id,
        status: 'passed',
        message: `${relative} exists; its code was not executed or validated.`,
      });
    } catch (error) {
      checks.push({ id, status: 'failed', message: description(error) });
    }
  }
  checks.push(...(await identityChecks(root, platform, options.identityTool)));
  /** @type {Record<string, string>} */
  const packages = {
    ...CONSUMER_CONTRACT.packages,
    ...(platform === 'marathon' ? CONSUMER_CONTRACT.marathonPackages : {}),
  };
  if (validManifest) {
    const require = createRequire(path.join(root, 'package.json'));
    for (const [name, expected] of Object.entries(packages)) {
      try {
        const resolved = require.resolve(`${name}/package.json`);
        const installed = await readMetadata(resolved);
        const matches = installed.name === name && installed.version === expected;
        checks.push({
          id: `package:${name}`,
          status: matches ? 'passed' : 'failed',
          expected,
          observed: `${installed.name ?? 'unnamed'}@${installed.version ?? 'unversioned'}`,
          message: matches
            ? 'Installed package metadata matches the reviewed baseline.'
            : 'Align the package with the reviewed platform graph; do not upgrade one ABI dependency in isolation.',
        });
      } catch (error) {
        const reason =
          error instanceof Error && 'code' in error ? String(error.code) : 'INVALID_METADATA';
        checks.push({
          id: `package:${name}`,
          status: 'failed',
          expected,
          message: `Cannot inspect installed package metadata (${reason}). Check installation or package exports; absence was not assumed.`,
        });
      }
    }
    try {
      const modules = await discoverNitroModules(root, platform);
      checks.push({
        id: 'nitro-modules',
        status: 'passed',
        observed:
          modules.map((module) => `${module.package}@${module.version}`).join(', ') || 'none',
        message: modules.length
          ? 'Nitro module declarations in package.json are well formed (docs/adr/0019); nothing was compiled.'
          : 'No direct dependency declares a Nitro module for this platform.',
      });
    } catch (error) {
      checks.push({ id: 'nitro-modules', status: 'failed', message: description(error) });
    }
  } else {
    checks.push({
      id: 'package-graph',
      status: 'not-checked',
      message: 'Fix the project manifest before package resolution.',
    });
  }
  for (const [id, message] of [
    [
      'native-toolchain',
      'QNX SDP, Qt, libc, compiler, plugins and target binary were not inspected.',
    ],
    [
      'native-modules',
      'Third-party native dependency support, Nitro ABI, registration and permissions were not audited.',
    ],
    [
      'runtime',
      'No bundling, device connection, native launch, Router or lifecycle behavior was tested.',
    ],
    [
      'debugger',
      'No inspector endpoint, attach, breakpoint, source map or Fast Refresh was tested.',
    ],
  ])
    checks.push({ id, status: 'not-checked', message });
  return {
    schemaVersion: 1,
    platform,
    project: root,
    baseline: CONSUMER_CONTRACT.baseline,
    metadataCompatible: checks.every((check) => check.status !== 'failed'),
    nativeBuildVerified: false,
    runtimeVerified: false,
    checks,
  };
}

/** @param {string[]} argv */
export async function doctorMain(argv) {
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
      'identity-tool': { type: 'string' },
      json: { type: 'boolean' },
    },
  });
  const seen = new Set();
  for (const token of tokens) {
    if (token.kind !== 'option') continue;
    if (seen.has(token.name)) throw new Error(`Duplicate option --${token.name}.`);
    seen.add(token.name);
  }
  if (values.help) {
    console.log(help);
    return 0;
  }
  if (positionals.length) throw new Error('doctor accepts named options only.');
  const report = await inspectConsumer(values.project ?? '.', {
    platform: values.platform,
    entry: values.entry,
    identityTool: values['identity-tool'],
  });
  if (values.json) console.log(JSON.stringify(report, null, 2));
  else {
    console.log(`React Native QNX project metadata (${report.platform})`);
    for (const check of report.checks)
      console.log(`${check.status}: ${check.id} — ${check.message}`);
    console.log(
      report.metadataCompatible
        ? 'Inspected metadata matches; native and runtime checks remain unverified.'
        : 'Fix the failed checks before attempting this platform workflow.'
    );
  }
  return report.metadataCompatible ? 0 : 1;
}
if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    process.exitCode = await doctorMain(process.argv.slice(2));
  } catch (error) {
    console.error(`rnqnx doctor: ${description(error)}`);
    process.exitCode = 2;
  }
}
