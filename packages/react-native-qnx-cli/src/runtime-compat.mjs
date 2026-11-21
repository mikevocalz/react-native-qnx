import { open, readFile } from 'node:fs/promises';
import path from 'node:path';
import { locatePackage } from './nitro-modules.mjs';

// Runner and bundle compatibility. A prebuilt runner is one native host built
// from one React Native, one Hermes and one Nitro runtime. A bundle only runs
// on it when all three agree, so the launch step compares them before anything
// is copied to the board. docs/packaging.md#runner-runtime-descriptor defines
// the descriptor; docs/v1/design/strings.md holds the message text.

export const RUNTIME_SCHEMA = 'rnqnx-runtime/v1';
export const RUNTIME_FILE = 'rnqnx-runtime.json';

/** Hermes bytecode file header: a 64-bit magic, then a 32-bit version, both
 * little-endian (hermes lib/BCGen/HBC/BytecodeVersion.h, BytecodeFileFormat.h).
 * Magic 0x1F1903C103BC1FC6. */
const HBC_MAGIC = Buffer.from('c61fbc03c103191f', 'hex');
const HBC_HEADER_BYTES = 12;
const PACKAGE_NAME = /^(?:@[a-z0-9-~][a-z0-9-._~]*\/)?[a-z0-9-~][a-z0-9-._~]*$/;
const VERSION = /^[0-9A-Za-z][0-9A-Za-z.+_-]{0,127}$/;
const MAX_ENTRIES = 256;

/** @typedef {{package: string, version: string}} PackageRef */
/** @typedef {{nitroModules: PackageRef[], expoModules: PackageRef[], fabricComponents: PackageRef[]}} Builtins */
/** @typedef {{hermesBytecodeVersion: number, nitroAbi: string, builtins: Builtins}} RuntimeInfo */
/** @typedef {RuntimeInfo & {schema: string, runnerId: string, target: string, versions: Record<string, string | null>}} RuntimeDescriptor */
/** @typedef {{reactNative: string | null, hermesBytecodeVersion: number | null, nitroAbi: string | null, bundleFile: string | null}} BundleProfile */
/** @typedef {{field: 'reactNative' | 'hermesBytecodeVersion' | 'nitroAbi', label: string, runner: string, bundle: string}} Mismatch */

/** @param {unknown} value @returns {value is Record<string, unknown>} */
function isObject(value) {
  return Boolean(value) && typeof value === 'object' && !Array.isArray(value);
}

/** @param {unknown} value @param {string} where @returns {PackageRef[]} */
function packageRefs(value, where) {
  if (!Array.isArray(value) || value.length > MAX_ENTRIES)
    throw new Error(`${where} must be an array of at most ${MAX_ENTRIES} entries.`);
  const seen = new Set();
  return value.map((item, index) => {
    if (!isObject(item)) throw new Error(`${where}[${index}] must be an object.`);
    const { package: name, version, ...rest } = item;
    if (Object.keys(rest).length)
      throw new Error(`${where}[${index}] has unknown keys: ${Object.keys(rest).join(', ')}.`);
    if (typeof name !== 'string' || !PACKAGE_NAME.test(name) || name.length > 214)
      throw new Error(`${where}[${index}].package is not a valid npm package name.`);
    if (typeof version !== 'string' || !VERSION.test(version))
      throw new Error(`${where}[${index}].version is missing or invalid.`);
    if (seen.has(name)) throw new Error(`${where} lists ${name} twice.`);
    seen.add(name);
    return { package: name, version };
  });
}

/**
 * The part of a descriptor that decides compatibility. It is carried both in
 * a manifest entry (`runtime`) and, with identity fields, in the archive's
 * rnqnx-runtime.json.
 * @param {unknown} input @param {string} where @returns {RuntimeInfo}
 */
export function validateRuntimeInfo(input, where) {
  if (!isObject(input)) throw new Error(`${where} must be an object.`);
  const { hermesBytecodeVersion, nitroAbi, builtins, ...rest } = input;
  const extra = Object.keys(rest).filter(
    (key) => !['schema', 'runnerId', 'target', 'versions'].includes(key)
  );
  if (extra.length) throw new Error(`${where} has unknown keys: ${extra.join(', ')}.`);
  if (
    !Number.isInteger(hermesBytecodeVersion) ||
    /** @type {number} */ (hermesBytecodeVersion) < 1 ||
    /** @type {number} */ (hermesBytecodeVersion) > 0xffffffff
  )
    throw new Error(`${where}.hermesBytecodeVersion must be a positive integer.`);
  if (typeof nitroAbi !== 'string' || !VERSION.test(nitroAbi))
    throw new Error(`${where}.nitroAbi must be the exact Nitro runtime version string.`);
  if (!isObject(builtins)) throw new Error(`${where}.builtins must be an object.`);
  const { nitroModules, expoModules, fabricComponents, ...unknown } = builtins;
  if (Object.keys(unknown).length)
    throw new Error(`${where}.builtins has unknown keys: ${Object.keys(unknown).join(', ')}.`);
  return {
    hermesBytecodeVersion: /** @type {number} */ (hermesBytecodeVersion),
    nitroAbi,
    builtins: {
      nitroModules: packageRefs(nitroModules, `${where}.builtins.nitroModules`),
      expoModules: packageRefs(expoModules, `${where}.builtins.expoModules`),
      fabricComponents: packageRefs(fabricComponents, `${where}.builtins.fabricComponents`),
    },
  };
}

/** @param {unknown} input @returns {RuntimeDescriptor} */
export function validateRuntimeDescriptor(input) {
  if (!isObject(input)) throw new Error(`${RUNTIME_FILE} must be a JSON object.`);
  if (input.schema !== RUNTIME_SCHEMA)
    throw new Error(`${RUNTIME_FILE} has an unsupported schema; expected ${RUNTIME_SCHEMA}.`);
  if (typeof input.runnerId !== 'string' || !input.runnerId)
    throw new Error(`${RUNTIME_FILE}.runnerId must be a string.`);
  if (typeof input.target !== 'string' || !input.target)
    throw new Error(`${RUNTIME_FILE}.target must be a string.`);
  if (!isObject(input.versions)) throw new Error(`${RUNTIME_FILE}.versions must be an object.`);
  const versions = /** @type {Record<string, string | null>} */ (input.versions);
  if (typeof versions.reactNative !== 'string' || !VERSION.test(versions.reactNative))
    throw new Error(`${RUNTIME_FILE}.versions.reactNative is missing or invalid.`);
  return {
    schema: RUNTIME_SCHEMA,
    runnerId: input.runnerId,
    target: input.target,
    versions,
    ...validateRuntimeInfo(input, RUNTIME_FILE),
  };
}

/** @param {string} file @returns {Promise<RuntimeDescriptor>} */
export async function readRuntimeDescriptor(file) {
  let text;
  try {
    text = await readFile(file, 'utf8');
  } catch (error) {
    throw new Error(
      `Cannot read the runner details at ${file} (${error instanceof Error ? error.message : error}).`
    );
  }
  let parsed;
  try {
    parsed = JSON.parse(text);
  } catch {
    throw new Error(`${file} is not valid JSON.`);
  }
  return validateRuntimeDescriptor(parsed);
}

/**
 * The Hermes bytecode version of a compiled bundle, or null when the file is
 * not Hermes bytecode (a JavaScript source bundle).
 * @param {string} file @returns {Promise<number | null>}
 */
export async function readHermesBytecodeVersion(file) {
  const handle = await open(file, 'r');
  try {
    const header = Buffer.alloc(HBC_HEADER_BYTES);
    const { bytesRead } = await handle.read(header, 0, HBC_HEADER_BYTES, 0);
    if (bytesRead < HBC_HEADER_BYTES || !header.subarray(0, 8).equals(HBC_MAGIC)) return null;
    return header.readUInt32LE(8);
  } finally {
    await handle.close();
  }
}

/** @param {string} root @param {string} name @returns {Promise<string | null>} */
async function installedVersion(root, name) {
  const packageRoot = await locatePackage(root, name);
  if (!packageRoot) return null;
  const manifest = JSON.parse(await readFile(path.join(packageRoot, 'package.json'), 'utf8'));
  return typeof manifest.version === 'string' ? manifest.version : null;
}

/**
 * The Nitro runtime the app's bundle was built against: the installed
 * `nitro-core` version, else `react-native-nitro-modules`. Nitro 1.x is an
 * alpha with no ABI promise (AGENTS.md), so the exact version string is the
 * ABI identity. Null when the app depends on neither.
 * @param {string} root @returns {Promise<string | null>}
 */
export async function installedNitroAbi(root) {
  const core = await installedVersion(root, 'nitro-core');
  const modules = await installedVersion(root, 'react-native-nitro-modules');
  if (core && modules && core !== modules)
    throw new Error(
      `The app installs nitro-core ${core} and react-native-nitro-modules ${modules}. Install one Nitro version so the runner can be matched to it.`
    );
  return core ?? modules;
}

/**
 * What the bundle needs from a runner.
 * @param {string} root realpath of the project @param {string | undefined} bundleFile
 * @returns {Promise<BundleProfile>}
 */
export async function bundleProfile(root, bundleFile) {
  return {
    reactNative: await installedVersion(root, 'react-native'),
    hermesBytecodeVersion: bundleFile ? await readHermesBytecodeVersion(bundleFile) : null,
    nitroAbi: await installedNitroAbi(root),
    bundleFile: bundleFile ?? null,
  };
}

/**
 * Compare a runner with a bundle on the three fields that decide whether the
 * bundle can run. A field the bundle does not use (no Nitro dependency, a
 * JavaScript source bundle) is reported in `notChecked`, never as a pass.
 * @param {{versions: Record<string, string | null>} & RuntimeInfo} runtime
 * @param {BundleProfile} bundle
 * @returns {{ok: boolean, compared: number, mismatches: Mismatch[], notChecked: string[]}}
 */
export function compareRunnerToBundle(runtime, bundle) {
  /** @type {Mismatch[]} */
  const mismatches = [];
  /** @type {string[]} */
  const notChecked = [];
  if (bundle.reactNative === null)
    notChecked.push('React Native version: react-native is not installed in the project.');
  else if (bundle.reactNative !== runtime.versions.reactNative)
    mismatches.push({
      field: 'reactNative',
      label: 'React Native',
      runner: String(runtime.versions.reactNative),
      bundle: bundle.reactNative,
    });
  if (bundle.hermesBytecodeVersion === null)
    notChecked.push(
      bundle.bundleFile
        ? 'Hermes bytecode version: the bundle is JavaScript source, not Hermes bytecode.'
        : 'Hermes bytecode version: no bundle was given.'
    );
  else if (bundle.hermesBytecodeVersion !== runtime.hermesBytecodeVersion)
    mismatches.push({
      field: 'hermesBytecodeVersion',
      label: 'Hermes bytecode',
      runner: String(runtime.hermesBytecodeVersion),
      bundle: String(bundle.hermesBytecodeVersion),
    });
  if (bundle.nitroAbi === null) notChecked.push('Nitro ABI: the app does not depend on Nitro.');
  else if (bundle.nitroAbi !== runtime.nitroAbi)
    mismatches.push({
      field: 'nitroAbi',
      label: 'Nitro ABI',
      runner: runtime.nitroAbi,
      bundle: bundle.nitroAbi,
    });
  return {
    ok: mismatches.length === 0,
    compared: 3 - notChecked.length,
    mismatches,
    notChecked,
  };
}

/**
 * The launch error. Names both sides of every mismatch and the next step;
 * text is `launch.*` in docs/v1/design/strings.md.
 * @param {string} runnerId @param {Mismatch[]} mismatches
 */
export function formatMismatch(runnerId, mismatches) {
  const width = Math.max(...mismatches.map((m) => m.label.length));
  const rows = mismatches.map(
    (m) => `  ${m.label.padEnd(width)}  runner ${m.runner}  app ${m.bundle}`
  );
  return [
    `This runner cannot load this app (runner ${runnerId}).`,
    ...rows,
    'Use a runner built for this app, or build your own: rnqnx project build-native --help.',
  ].join('\n');
}
