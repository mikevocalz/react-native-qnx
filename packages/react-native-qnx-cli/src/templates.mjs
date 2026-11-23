import { createHash } from 'node:crypto';
import { lstat, readFile, readdir, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { gunzipSync, gzipSync } from 'node:zlib';

/**
 * Versioned template tarballs (hardening row H14).
 *
 * Each directory under template/ named in template/templates.json is packed into
 * a deterministic ustar archive (sorted names, mode 0644, uid/gid 0, mtime 0)
 * and gzipped. The content hash is the SHA-256 of the uncompressed tar: it
 * depends only on the template files, not on the zlib build. The tarball hash is
 * the SHA-256 of the .tgz and can differ between zlib versions, so it is
 * reported but not pinned. template/templates.lock.json pins version and content
 * hash; a template edited without a version bump and a lock update is refused.
 *
 * Host only. Nothing here touches a QNX target.
 */

export const sourceRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
const LIMITS = { files: 64, fileBytes: 1024 * 1024, totalBytes: 4 * 1024 * 1024, name: 100 };

/** @typedef {{version: string, description: string, commands: string[], default: string[], extract: string[] | null}} TemplateEntry */
/** @typedef {{name: string, version: string, files: {path: string, size: number}[], contentSha256: string, tarballSha256: string, tarball: Buffer}} PackedTemplate */

/** @param {string} root */
export async function loadRegistry(root = sourceRoot) {
  const registry = JSON.parse(
    await readFile(path.join(root, 'template', 'templates.json'), 'utf8')
  );
  if (registry.schemaVersion !== 1 || typeof registry.templates !== 'object')
    throw new Error('template/templates.json has an unsupported schema.');
  /** @type {Record<string, TemplateEntry>} */
  const templates = registry.templates;
  for (const [name, entry] of Object.entries(templates)) {
    if (!/^[a-z][a-z0-9-]{0,40}$/.test(name)) throw new Error('Invalid template name: ' + name);
    if (!/^\d+\.\d+\.\d+$/.test(entry.version))
      throw new Error('Template ' + name + ' needs an exact x.y.z version.');
  }
  return templates;
}

/** @param {string} root */
export async function loadLock(root = sourceRoot) {
  const lock = JSON.parse(
    await readFile(path.join(root, 'template', 'templates.lock.json'), 'utf8')
  );
  if (lock.schemaVersion !== 1)
    throw new Error('template/templates.lock.json has an unsupported schema.');
  /** @type {Record<string, {version: string, contentSha256: string}>} */
  return lock.templates;
}

/** @param {string} dir @param {string} rel @param {{path: string, data: Buffer}[]} out */
async function walk(dir, rel, out) {
  for (const entry of (await readdir(path.join(dir, rel))).sort()) {
    const next = rel ? rel + '/' + entry : entry;
    const full = path.join(dir, next);
    const stat = await lstat(full);
    if (stat.isSymbolicLink()) throw new Error('Template contains a symlink: ' + next);
    if (stat.isDirectory()) await walk(dir, next, out);
    else if (stat.isFile()) {
      if (stat.size > LIMITS.fileBytes) throw new Error('Template file too large: ' + next);
      out.push({ path: next, data: await readFile(full) });
    } else throw new Error('Template contains a special file: ' + next);
  }
}

/** @param {string} name */
function checkEntryName(name) {
  if (
    !name ||
    name.length > LIMITS.name ||
    name.startsWith('/') ||
    name.includes('\\') ||
    name.includes('\0') ||
    name.split('/').some((part) => part === '' || part === '.' || part === '..')
  )
    throw new Error('Unsafe template entry name: ' + JSON.stringify(name));
}

/** @param {number} value @param {number} width */
function octal(value, width) {
  return value.toString(8).padStart(width - 1, '0') + '\0';
}

/** @param {{path: string, data: Buffer}[]} files */
export function writeTar(files) {
  const blocks = [];
  for (const file of files) {
    checkEntryName(file.path);
    const header = Buffer.alloc(512);
    header.write(file.path, 0, 'utf8');
    header.write(octal(0o644, 8), 100, 'ascii');
    header.write(octal(0, 8), 108, 'ascii');
    header.write(octal(0, 8), 116, 'ascii');
    header.write(octal(file.data.length, 12), 124, 'ascii');
    header.write(octal(0, 12), 136, 'ascii');
    header.fill(' ', 148, 156);
    header.write('0', 156, 'ascii');
    header.write('ustar\0', 257, 'ascii');
    header.write('00', 263, 'ascii');
    const sum = header.reduce((total, byte) => total + byte, 0);
    header.write(sum.toString(8).padStart(6, '0') + '\0 ', 148, 'ascii');
    blocks.push(header, file.data, Buffer.alloc((512 - (file.data.length % 512)) % 512));
  }
  blocks.push(Buffer.alloc(1024));
  return Buffer.concat(blocks);
}

/** @param {Buffer} tar @returns {Map<string, Buffer>} */
export function readTar(tar) {
  /** @type {Map<string, Buffer>} */
  const files = new Map();
  let offset = 0;
  let total = 0;
  for (;;) {
    if (offset + 512 > tar.length) throw new Error('Template archive is truncated.');
    const header = tar.subarray(offset, offset + 512);
    if (header.every((byte) => byte === 0)) break;
    const stored = parseInt(header.toString('ascii', 148, 154), 8);
    const copy = Buffer.from(header);
    copy.fill(' ', 148, 156);
    if (copy.reduce((sum, byte) => sum + byte, 0) !== stored)
      throw new Error('Template archive header checksum mismatch.');
    const type = String.fromCharCode(header[156]);
    if (type !== '0' && type !== '\0') throw new Error('Template archive holds a non-file entry.');
    const nameEnd = header.indexOf(0);
    const name = header.toString('utf8', 0, nameEnd === -1 || nameEnd > 100 ? 100 : nameEnd);
    checkEntryName(name);
    const size = parseInt(header.toString('ascii', 124, 135), 8);
    if (!Number.isInteger(size) || size < 0 || size > LIMITS.fileBytes)
      throw new Error('Template archive entry has an invalid size.');
    total += size;
    if (files.has(name) || files.size >= LIMITS.files || total > LIMITS.totalBytes)
      throw new Error('Template archive exceeds its bounds or repeats an entry.');
    const start = offset + 512;
    if (start + size > tar.length) throw new Error('Template archive is truncated.');
    files.set(name, Buffer.from(tar.subarray(start, start + size)));
    offset = start + Math.ceil(size / 512) * 512;
  }
  return files;
}

/** @param {Buffer} data */
const sha256 = (data) => createHash('sha256').update(data).digest('hex');

/** @param {string} name @param {string} root @returns {Promise<PackedTemplate>} */
export async function packTemplate(name, root = sourceRoot) {
  const registry = await loadRegistry(root);
  const entry = registry[name];
  if (!entry) throw new Error('Unknown template: ' + name);
  /** @type {{path: string, data: Buffer}[]} */
  const files = [];
  await walk(path.join(root, 'template'), name, files);
  const prefix = name + '/';
  const rooted = files.map((file) => ({ path: file.path.slice(prefix.length), data: file.data }));
  if (rooted.length === 0 || rooted.length > LIMITS.files)
    throw new Error('Template ' + name + ' has no files or too many.');
  rooted.sort((a, b) => (a.path < b.path ? -1 : a.path > b.path ? 1 : 0));
  const tar = writeTar(rooted);
  const tarball = gzipSync(tar, { level: 9 });
  return {
    name,
    version: entry.version,
    files: rooted.map((file) => ({ path: file.path, size: file.data.length })),
    contentSha256: sha256(tar),
    tarballSha256: sha256(tarball),
    tarball,
  };
}

/**
 * Packs a template and refuses it if version or content differ from the lock.
 * @param {string} name @param {string} root
 */
export async function packVerified(name, root = sourceRoot) {
  const packed = await packTemplate(name, root);
  const pinned = (await loadLock(root))[name];
  if (!pinned)
    throw new Error('Template ' + name + ' is missing from template/templates.lock.json.');
  if (pinned.version !== packed.version)
    throw new Error(
      'Template ' + name + ' is at ' + packed.version + ' but the lock pins ' + pinned.version + '.'
    );
  if (pinned.contentSha256 !== packed.contentSha256)
    throw new Error(
      'Template ' +
        name +
        ' changed without a version bump (content ' +
        packed.contentSha256.slice(0, 12) +
        ' vs locked ' +
        pinned.contentSha256.slice(0, 12) +
        '). Bump its version in template/templates.json and run: pnpm qnx templates lock'
    );
  return packed;
}

/**
 * Extracts files from a packed template, re-checking the content hash first.
 * @param {PackedTemplate} packed @param {string[] | null} only
 */
export function extractTemplate(packed, only = null) {
  const tar = gunzipSync(packed.tarball, { maxOutputLength: LIMITS.totalBytes + 1024 * 1024 });
  if (sha256(tar) !== packed.contentSha256)
    throw new Error('Template archive content hash mismatch.');
  const files = readTar(tar);
  if (!only) return files;
  /** @type {Map<string, Buffer>} */
  const picked = new Map();
  for (const file of only) {
    const data = files.get(file);
    if (!data) throw new Error('Template ' + packed.name + ' lacks ' + file);
    picked.set(file, data);
  }
  return picked;
}

/** The receipt line printed by create commands. @param {PackedTemplate} packed */
export function receipt(packed) {
  return {
    template: packed.name,
    version: packed.version,
    contentSha256: packed.contentSha256,
    tarballSha256: packed.tarballSha256,
    tarballBytes: packed.tarball.length,
    files: packed.files.length,
  };
}

/**
 * Picks the template for a command. A command's default applies when none is named.
 * @param {string} command @param {string | undefined} requested @param {string} root
 */
export async function selectTemplate(command, requested, root = sourceRoot) {
  const registry = await loadRegistry(root);
  const eligible = Object.entries(registry).filter(([, entry]) => entry.commands.includes(command));
  const name = requested ?? eligible.find(([, entry]) => entry.default.includes(command))?.[0];
  if (!name || !eligible.some(([candidate]) => candidate === name))
    throw new Error(
      'Unknown template' +
        (requested ? ' ' + JSON.stringify(requested) : '') +
        ' for ' +
        command +
        '. Available: ' +
        eligible.map(([candidate]) => candidate).join(', ')
    );
  return { name, entry: registry[name] };
}

/** Rewrites template/templates.lock.json from the current template files. @param {string} root */
export async function writeLock(root = sourceRoot) {
  const registry = await loadRegistry(root);
  /** @type {Record<string, {version: string, contentSha256: string}>} */
  const templates = {};
  for (const name of Object.keys(registry).sort()) {
    const packed = await packTemplate(name, root);
    templates[name] = { version: packed.version, contentSha256: packed.contentSha256 };
  }
  await writeFile(
    path.join(root, 'template', 'templates.lock.json'),
    JSON.stringify({ schemaVersion: 1, templates }, null, 2) + '\n'
  );
  return templates;
}
