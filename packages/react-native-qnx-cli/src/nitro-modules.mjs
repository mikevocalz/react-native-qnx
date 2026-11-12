// Discovery of Nitro modules that declare a QNX native build in their own
// package.json. The format is defined in docs/adr/0019. Discovery reads JSON
// only: no package, config or CMake code runs here.
import { lstat, readFile, realpath } from 'node:fs/promises';
import path from 'node:path';
import { absent, boundedFile, projectPath } from './project-path.mjs';

/** The package.json field that holds the declaration. */
export const NITRO_METADATA_FIELD = 'react-native-qnx';
/** Platforms the declaration may name today. Marathon has no Nitro runtime
 * in the consumer build yet, so it is rejected instead of silently ignored. */
const SUPPORTED_PLATFORMS = ['qnx'];
const MAX_MODULES = 64;
const MAX_MANIFEST_BYTES = 1024 * 1024;
const PACKAGE_NAME = /^(?:@[a-z0-9-~][a-z0-9-._~]*\/)?[a-z0-9-~][a-z0-9-._~]*$/;
const CMAKE_TARGET = /^[A-Za-z_][A-Za-z0-9_.+-]*$/;
const CXX_FUNCTION = /^[A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_][A-Za-z0-9_]*)*$/;
const CMAKE_UNSAFE = /[\x00-\x1f\\;"$]/;

/** @typedef {{platforms: string[], cmakeListsPath: string, target: string, initializer: string}} NitroDeclaration */
/** @typedef {{package: string, version: string, root: string, cmakeLists: string, target: string, initializer: string}} NitroModule */

/** @param {unknown} value @returns {value is Record<string, unknown>} */
function isObject(value) {
  return Boolean(value) && typeof value === 'object' && !Array.isArray(value);
}

/** Validate the `react-native-qnx.nitro` object of one package.
 * @param {string} name package name, used in error messages
 * @param {unknown} field the whole `react-native-qnx` value
 * @returns {NitroDeclaration} */
export function validateNitroDeclaration(name, field) {
  const where = `${name}: package.json "${NITRO_METADATA_FIELD}"`;
  if (!isObject(field)) throw new Error(`${where} must be an object.`);
  for (const key of Object.keys(field))
    if (key !== 'nitro') throw new Error(`${where}.${key} is not a supported key; use "nitro".`);
  const nitro = field.nitro;
  if (!isObject(nitro)) throw new Error(`${where}.nitro must be an object.`);
  const allowed = ['platforms', 'cmakeListsPath', 'target', 'initializer'];
  for (const key of Object.keys(nitro))
    if (!allowed.includes(key))
      throw new Error(`${where}.nitro.${key} is not supported. Allowed: ${allowed.join(', ')}.`);
  const { platforms, cmakeListsPath, target, initializer } = nitro;
  if (!Array.isArray(platforms) || platforms.length === 0)
    throw new Error(`${where}.nitro.platforms must be a nonempty array, for example ["qnx"].`);
  for (const platform of platforms)
    if (typeof platform !== 'string' || !SUPPORTED_PLATFORMS.includes(platform))
      throw new Error(
        `${where}.nitro.platforms contains ${JSON.stringify(platform)}; supported: ${SUPPORTED_PLATFORMS.join(', ')}.`
      );
  if (new Set(platforms).size !== platforms.length)
    throw new Error(`${where}.nitro.platforms must not repeat a platform.`);
  if (
    typeof cmakeListsPath !== 'string' ||
    !cmakeListsPath ||
    CMAKE_UNSAFE.test(cmakeListsPath) ||
    path.posix.isAbsolute(cmakeListsPath) ||
    path.win32.isAbsolute(cmakeListsPath) ||
    cmakeListsPath.split('/').some((part) => !part || part === '.' || part === '..') ||
    path.posix.basename(cmakeListsPath) !== 'CMakeLists.txt'
  )
    throw new Error(
      `${where}.nitro.cmakeListsPath must be a package-relative path to a CMakeLists.txt file, without "..", backslashes or CMake metacharacters.`
    );
  if (typeof target !== 'string' || !CMAKE_TARGET.test(target) || target.length > 128)
    throw new Error(`${where}.nitro.target must be a CMake target name of 1-128 characters.`);
  if (/^rnqnx_/i.test(target))
    throw new Error(`${where}.nitro.target must not use the reserved rnqnx_ prefix.`);
  if (
    typeof initializer !== 'string' ||
    !CXX_FUNCTION.test(initializer) ||
    initializer.length > 256
  )
    throw new Error(
      `${where}.nitro.initializer must name a C++ function such as "margelo::nitro::battery::registerQnxHybridClasses".`
    );
  return { platforms: [...platforms], cmakeListsPath, target, initializer };
}

/** @param {string} file */
async function readManifest(file) {
  await boundedFile(file, MAX_MANIFEST_BYTES);
  const value = JSON.parse(await readFile(file, 'utf8'));
  if (!isObject(value)) throw new Error(`${file} must contain a JSON object.`);
  return value;
}

/** Find node_modules/<name> the way Node's lookup walks parent directories,
 * without consulting package "exports" or running any resolver hooks.
 * @param {string} root @param {string} name */
async function locatePackage(root, name) {
  let directory = root;
  for (;;) {
    const candidate = path.join(directory, 'node_modules', ...name.split('/'));
    try {
      if ((await lstat(candidate)) && (await lstat(path.join(candidate, 'package.json'))))
        return realpath(candidate);
    } catch (error) {
      if (!absent(error)) throw error;
    }
    const parent = path.dirname(directory);
    if (parent === directory) return undefined;
    directory = parent;
  }
}

/** Discover the app's direct `dependencies` that declare a Nitro module for
 * the selected platform. Packages without the field, and dependencies not
 * installed under node_modules, are skipped. A package
 * with a malformed field stops discovery with an error naming the package.
 * @param {string} root realpath of the consumer project
 * @param {string} platform
 * @returns {Promise<NitroModule[]>} */
export async function discoverNitroModules(root, platform) {
  const manifest = await readManifest(path.join(root, 'package.json'));
  const dependencies = manifest.dependencies ?? {};
  if (!isObject(dependencies))
    throw new Error('Project package.json "dependencies" must be an object.');
  /** @type {NitroModule[]} */
  const modules = [];
  for (const name of Object.keys(dependencies).sort()) {
    if (name.length > 214 || !PACKAGE_NAME.test(name))
      throw new Error(`Dependency name ${JSON.stringify(name)} is not a valid npm package name.`);
    // A dependency that is not installed has no native code to add. Installing
    // it later changes discovery, and build-native then reports the descriptor
    // as stale.
    const packageRoot = await locatePackage(root, name);
    if (!packageRoot) continue;
    const metadata = await readManifest(path.join(packageRoot, 'package.json'));
    if (metadata[NITRO_METADATA_FIELD] === undefined) continue;
    if (metadata.name !== name)
      throw new Error(`node_modules/${name} contains package ${JSON.stringify(metadata.name)}.`);
    if (typeof metadata.version !== 'string' || !metadata.version)
      throw new Error(`${name}: package.json must declare a version.`);
    const declaration = validateNitroDeclaration(name, metadata[NITRO_METADATA_FIELD]);
    if (!declaration.platforms.includes(platform)) continue;
    if (CMAKE_UNSAFE.test(packageRoot))
      throw new Error(`${name}: installed path contains characters CMake cannot take.`);
    const cmakeLists = await projectPath(packageRoot, declaration.cmakeListsPath);
    try {
      await boundedFile(cmakeLists, MAX_MANIFEST_BYTES);
    } catch (error) {
      throw new Error(
        `${name}: ${declaration.cmakeListsPath} must be a regular nonempty file inside the package (${error instanceof Error ? error.message : error}).`
      );
    }
    modules.push({
      package: name,
      version: metadata.version,
      root: packageRoot,
      cmakeLists,
      target: declaration.target,
      initializer: declaration.initializer,
    });
  }
  if (modules.length > MAX_MODULES)
    throw new Error(`At most ${MAX_MODULES} Nitro modules can be linked into one app.`);
  for (const key of /** @type {const} */ (['target', 'initializer'])) {
    const seen = new Map();
    for (const module of modules) {
      const other = seen.get(module[key]);
      if (other)
        throw new Error(
          `${other} and ${module.package} declare the same Nitro ${key} ${module[key]}.`
        );
      seen.set(module[key], module.package);
    }
  }
  return modules;
}
