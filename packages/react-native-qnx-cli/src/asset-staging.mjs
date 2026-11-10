import { createHash } from 'node:crypto';
import { createReadStream } from 'node:fs';
import { copyFile, lstat, mkdir, readFile, realpath } from 'node:fs/promises';
import path from 'node:path';

export const ASSET_INVENTORY_GENERATOR = 'react-native-qnx/metro-assets-v1';
export const ASSET_LIMITS = Object.freeze({
  assets: 4096,
  fileBytes: 64 * 1024 * 1024,
  totalBytes: 512 * 1024 * 1024,
  inventoryBytes: 4 * 1024 * 1024,
});
const SEGMENT = /^[A-Za-z0-9_][A-Za-z0-9_.+-]{0,127}$/;
const FILE_SEGMENT = /^[A-Za-z0-9_][A-Za-z0-9_.+@-]{0,159}$/;
const TYPE = /^[A-Za-z0-9]{1,16}$/;
const HEX = /^[0-9a-f]{64}$/;

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

/** @param {string} file */
export async function sha256File(file) {
  const hash = createHash('sha256');
  for await (const chunk of createReadStream(file)) hash.update(chunk);
  return hash.digest('hex');
}

/** @param {unknown} value */
function scaleSuffix(value) {
  if (typeof value !== 'number' || !Number.isFinite(value) || value <= 0 || value > 16)
    throw new Error('Asset scale must be a finite number in (0, 16].');
  return value === 1 ? '' : `@${value}x`;
}

/**
 * Metro's httpServerLocation ("/assets/<dir>") mapped to React Native's
 * bundle-relative layout: assets/<dir>/<name>[@<scale>x].<type>.
 * @param {unknown} location @param {unknown} name @param {unknown} type @param {unknown} scale
 */
export function assetDestination(location, name, type, scale) {
  if (typeof location !== 'string' || !location.startsWith('/assets'))
    throw new Error('Asset httpServerLocation must start with /assets.');
  const segments = location.slice(1).split('/');
  if (segments[0] !== 'assets' || segments.slice(1).some((part) => !SEGMENT.test(part)))
    throw new Error(`Unsupported asset location ${JSON.stringify(location)}.`);
  if (typeof name !== 'string' || !SEGMENT.test(name) || name.includes('@'))
    throw new Error(`Unsupported asset name ${JSON.stringify(name)}.`);
  if (typeof type !== 'string' || !TYPE.test(type))
    throw new Error(`Unsupported asset type ${JSON.stringify(type)}.`);
  return [...segments, `${name}${scaleSuffix(scale)}.${type}`].join('/');
}

/** @param {string} relative */
export function validInventoryPath(relative) {
  if (typeof relative !== 'string' || relative.length > 1024) return false;
  const parts = relative.split('/');
  return (
    parts.length >= 2 &&
    parts[0] === 'assets' &&
    parts.slice(1).every((part) => FILE_SEGMENT.test(part))
  );
}

/**
 * Copy Metro's selected assets into a new staging directory.
 * Source files must be regular, symlink-free files inside the project and must
 * not change while they are copied.
 * @param {{project: string, staging: string, assets: unknown, platform: string, bundleSha256: string, metroVersion: string}} options
 */
export async function stageMetroAssets({
  project,
  staging,
  assets,
  platform,
  bundleSha256,
  metroVersion,
}) {
  if (!Array.isArray(assets)) throw new Error('Metro did not return an asset list.');
  if (assets.length > ASSET_LIMITS.assets) throw new Error('Too many assets.');
  if (!HEX.test(bundleSha256)) throw new Error('Bundle identity must be a SHA-256 digest.');
  const root = await realpath(project);
  /** @type {Map<string, string>} */
  const claimed = new Map();
  /** @type {{path: string, scale: number, size: number, sha256: string}[]} */
  const entries = [];
  let total = 0;
  for (const asset of assets) {
    if (!asset || typeof asset !== 'object') throw new Error('Invalid Metro asset record.');
    const { httpServerLocation, name, type, scales, files } = /** @type {any} */ (asset);
    if (
      !Array.isArray(scales) ||
      !Array.isArray(files) ||
      scales.length !== files.length ||
      !files.length
    )
      throw new Error(`Asset ${String(name)} has mismatched files and scales.`);
    for (const [index, source] of files.entries()) {
      if (typeof source !== 'string' || !path.isAbsolute(source))
        throw new Error('Metro asset files must be absolute paths.');
      const stat = await lstat(source);
      if (stat.isSymbolicLink() || !stat.isFile())
        throw new Error(`Asset ${source} is not a regular file.`);
      const real = await realpath(source);
      if (real !== source || !inside(root, real))
        throw new Error(`Asset ${source} must be a symlink-free file inside the project.`);
      if (stat.size > ASSET_LIMITS.fileBytes)
        throw new Error(`Asset ${source} exceeds its size limit.`);
      total += stat.size;
      if (total > ASSET_LIMITS.totalBytes) throw new Error('Assets exceed the total size limit.');
      const relative = assetDestination(httpServerLocation, name, type, scales[index]);
      const folded = relative.toLowerCase();
      const previous = claimed.get(folded);
      if (previous !== undefined && previous !== source)
        throw new Error(`Assets ${previous} and ${source} collide at ${relative}.`);
      if (previous !== undefined) continue;
      claimed.set(folded, source);
      const destination = path.join(staging, ...relative.split('/'));
      await mkdir(path.dirname(destination), { recursive: true });
      await copyFile(source, destination, 1 /* COPYFILE_EXCL */);
      const sha256 = await sha256File(destination);
      if (sha256 !== (await sha256File(source)))
        throw new Error(`Asset ${source} changed while staging.`);
      entries.push({ path: relative, scale: scales[index], size: stat.size, sha256 });
    }
  }
  entries.sort((left, right) => (left.path < right.path ? -1 : left.path > right.path ? 1 : 0));
  return {
    schemaVersion: 1,
    generatedBy: ASSET_INVENTORY_GENERATOR,
    platform,
    metroVersion,
    bundleSha256,
    assets: entries,
  };
}

/**
 * Re-verify a published inventory: shape, confined paths and every staged hash.
 * @param {string} inventoryFile
 */
export async function verifyAssetInventory(inventoryFile) {
  const stat = await lstat(inventoryFile);
  if (
    stat.isSymbolicLink() ||
    !stat.isFile() ||
    stat.size < 1 ||
    stat.size > ASSET_LIMITS.inventoryBytes
  )
    throw new Error('Asset inventory must be a regular file within its size limit.');
  const inventory = JSON.parse(await readFile(inventoryFile, 'utf8'));
  const keys = [
    'schemaVersion',
    'generatedBy',
    'platform',
    'metroVersion',
    'bundleSha256',
    'assets',
  ];
  if (
    !inventory ||
    typeof inventory !== 'object' ||
    Object.keys(inventory).sort().join() !== [...keys].sort().join()
  )
    throw new Error('Asset inventory has unexpected fields.');
  if (inventory.schemaVersion !== 1 || inventory.generatedBy !== ASSET_INVENTORY_GENERATOR)
    throw new Error('Unsupported asset inventory.');
  if (!['qnx', 'marathon'].includes(inventory.platform))
    throw new Error('Invalid asset inventory platform.');
  if (!HEX.test(inventory.bundleSha256))
    throw new Error('Invalid asset inventory bundle identity.');
  if (!Array.isArray(inventory.assets) || inventory.assets.length > ASSET_LIMITS.assets)
    throw new Error('Invalid asset inventory list.');
  const base = path.dirname(inventoryFile);
  const seen = new Set();
  for (const entry of inventory.assets) {
    if (
      !entry ||
      Object.keys(entry).sort().join() !== 'path,scale,sha256,size' ||
      !validInventoryPath(entry.path) ||
      !HEX.test(entry.sha256) ||
      !Number.isSafeInteger(entry.size) ||
      entry.size < 0
    )
      throw new Error('Invalid asset inventory entry.');
    scaleSuffix(entry.scale);
    const folded = entry.path.toLowerCase();
    if (seen.has(folded)) throw new Error(`Duplicate asset ${entry.path}.`);
    seen.add(folded);
    let current = base;
    for (const part of entry.path.split('/')) {
      current = path.join(current, part);
      const node = await lstat(current);
      if (node.isSymbolicLink())
        throw new Error(`Staged asset path ${entry.path} contains a symlink.`);
    }
    const file = await lstat(current);
    if (!file.isFile() || file.size !== entry.size || (await sha256File(current)) !== entry.sha256)
      throw new Error(`Staged asset ${entry.path} is missing or changed.`);
  }
  return inventory;
}
