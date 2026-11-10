import { createHash } from 'node:crypto';
import { readFile, realpath, stat } from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';

/** @typedef {{packageName: string, version: string, packageRoot: string, manifestSha256: string,
 * cmakeListsPath: string, target: string, registrationHeader: string, registrationFunction: string,
 * nitroCoreVersions: string[], requiresDispatcher: boolean}} NativePackageDescriptor */

const platforms = new Set(['qnx', 'marathon']);
const packageName = /^(?:@[a-z0-9][a-z0-9._-]*\/)?[a-z0-9][a-z0-9._-]*$/;
const targetName = /^[A-Za-z_][A-Za-z0-9_.+-]*$/;
const functionName = /^[A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_][A-Za-z0-9_]*)*$/;
const unsafe = /[\x00-\x1f\x7f\\;"$]/;

/** @param {unknown} error */
function missing(error) {
  return error instanceof Error && 'code' in error && error.code === 'ENOENT';
}
/** @param {string} file */
async function manifest(file) {
  const info = await stat(file);
  if (!info.isFile() || info.size < 1 || info.size > 1024 * 1024)
    throw new Error('Native dependency manifests must be regular files below 1 MiB.');
  const bytes = await readFile(file);
  const data = JSON.parse(bytes.toString('utf8'));
  if (!data || typeof data !== 'object' || Array.isArray(data))
    throw new Error('Invalid package manifest.');
  return { data, sha256: createHash('sha256').update(bytes).digest('hex') };
}
/** @param {string} name @param {string} importer @param {boolean} optional */
async function installedManifest(name, importer, optional) {
  if (!packageName.test(name)) throw new Error(`Invalid dependency name: ${name}`);
  // Resolve manifests without executing package entry points/configuration JS.
  // Searching Node's actual module paths also supports packages that do not
  // export package.json, nested installs, and pnpm's symlinked store.
  const require = createRequire(importer);
  for (const modules of require.resolve.paths(name) ?? []) {
    const candidate = path.join(modules, name, 'package.json');
    try {
      return await realpath(candidate);
    } catch (error) {
      if (!missing(error)) throw error;
    }
  }
  if (optional) return undefined;
  throw new Error(`Install dependency ${name} before preparing native packages.`);
}
/** @param {unknown} value @param {string} label */
function relativeFile(value, label) {
  if (
    typeof value !== 'string' ||
    !value ||
    unsafe.test(value) ||
    path.isAbsolute(value) ||
    path.win32.isAbsolute(value) ||
    value.split('/').some((part) => !part || part === '.' || part === '..')
  )
    throw new Error(`${label} must be a safe package-relative path.`);
  return value;
}
/** @param {string} root @param {Record<string, any>} data @param {string} sha256 @param {string} platform
 * @returns {Promise<NativePackageDescriptor | undefined>} */
async function descriptor(root, data, sha256, platform) {
  const metadata = data.rnqnxNative;
  if (!metadata) return undefined;
  if (metadata.schemaVersion !== 1 || !metadata.platforms || typeof metadata.platforms !== 'object')
    throw new Error(`${data.name}: unsupported rnqnxNative schema.`);
  const selected = metadata.platforms[platform];
  if (!selected) return undefined;
  if (
    unsafe.test(root) ||
    typeof data.name !== 'string' ||
    !packageName.test(data.name) ||
    typeof data.version !== 'string' ||
    !data.version ||
    unsafe.test(data.version)
  )
    throw new Error('Native package identity/path contains unsupported characters.');
  const cmakeListsPath = relativeFile(selected.cmakeListsPath, 'cmakeListsPath');
  if (path.basename(cmakeListsPath) !== 'CMakeLists.txt')
    throw new Error('Native packages must name CMakeLists.txt.');
  const resolvedCmake = await realpath(path.join(root, cmakeListsPath));
  if (!resolvedCmake.startsWith(root + path.sep) || !(await stat(resolvedCmake)).isFile())
    throw new Error('Native package CMake entry escapes its installed package.');
  if (
    typeof selected.target !== 'string' ||
    typeof selected.registrationFunction !== 'string' ||
    !targetName.test(selected.target) ||
    !functionName.test(selected.registrationFunction)
  )
    throw new Error('Native package has an invalid CMake target or C++ registration function.');
  const registrationHeader = relativeFile(selected.registrationHeader, 'registrationHeader');
  if (/[<>]/.test(registrationHeader)) throw new Error('Invalid registration header.');
  const nitroCoreVersions = selected.nitroCoreVersions;
  if (
    !Array.isArray(nitroCoreVersions) ||
    !nitroCoreVersions.length ||
    nitroCoreVersions.length > 8 ||
    nitroCoreVersions.some(
      (version) => typeof version !== 'string' || !/^[A-Za-z0-9.+-]+$/.test(version)
    )
  )
    throw new Error('Native Nitro packages must declare exact supported nitroCoreVersions.');
  if (typeof selected.requiresDispatcher !== 'boolean')
    throw new Error('Native packages must declare requiresDispatcher.');
  return {
    packageName: data.name,
    version: data.version,
    packageRoot: root,
    manifestSha256: sha256,
    cmakeListsPath,
    target: selected.target,
    registrationHeader,
    registrationFunction: selected.registrationFunction,
    nitroCoreVersions,
    requiresDispatcher: selected.requiresDispatcher,
  };
}

/** Discover only the installed production dependency graph selected by this app.
 * Metadata declares native source integration, not successful device acceptance.
 * Expo modules retain their separate Expo autolinking contract.
 * @param {string} project @param {string} platform
 * @returns {Promise<NativePackageDescriptor[]>}
 */
export async function discoverNativePackages(project, platform) {
  if (!platforms.has(platform)) throw new Error('Native package platform must be qnx or marathon.');
  const rootManifest = path.join(project, 'package.json');
  let root;
  try {
    root = await manifest(rootManifest);
  } catch (error) {
    if (missing(error)) return [];
    throw error;
  }
  const pending = [{ file: rootManifest, ...root, isApp: true }];
  const visited = new Set([rootManifest]);
  const found = [];
  while (pending.length) {
    const current = pending.shift();
    if (!current) break;
    if (!current.isApp) {
      const native = await descriptor(
        path.dirname(current.file),
        current.data,
        current.sha256,
        platform
      );
      if (native) found.push(native);
    }
    const dependencies = { ...current.data.dependencies, ...current.data.optionalDependencies };
    for (const name of Object.keys(dependencies).sort()) {
      const file = await installedManifest(
        name,
        current.file,
        Object.hasOwn(current.data.optionalDependencies ?? {}, name)
      );
      if (!file || visited.has(file)) continue;
      visited.add(file);
      if (visited.size > 10000)
        throw new Error('Native dependency discovery exceeds 10,000 installed packages.');
      pending.push({ file, ...(await manifest(file)), isApp: false });
    }
  }
  if (found.length > 64) throw new Error('Native package registration exceeds 64 modules.');
  found.sort((a, b) => a.packageName.localeCompare(b.packageName));
  /** @type {Array<'packageName' | 'target' | 'registrationFunction'>} */
  const identityFields = ['packageName', 'target', 'registrationFunction'];
  for (const field of identityFields) {
    if (new Set(found.map((entry) => entry[field])).size !== found.length)
      throw new Error(
        `Conflicting native package ${field}; install one version per native module.`
      );
  }
  return found;
}
