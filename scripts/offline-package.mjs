#!/usr/bin/env node
/** Offline payload packaging only. No target connection, execution or deployment. */
import { createHash } from 'node:crypto';
import { lstat, readdir, readFile, mkdir, writeFile, rename, rm, chmod } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

/** @typedef {{path:string, role:string, component:string, requires:string[], size:number, sha256:string, mode:number}} Entry */
const roles = new Set(['executable', 'bundle', 'asset', 'library', 'plugin', 'license']);
/** @param {string} value */
function safePath(value) {
  if (
    typeof value !== 'string' ||
    !value ||
    value.length > 1024 ||
    value.includes('\\') ||
    !/^[A-Za-z0-9_.+@/-]+$/.test(value) ||
    value.split('/').some((p) => !p || p === '.' || p === '..') ||
    value === 'manifest.json'
  )
    throw new Error(`Unsafe payload path: ${value}`);
  return value;
}
/** @param {string} root @param {string} relative */
async function regular(root, relative) {
  safePath(relative);
  const pieces = relative.split('/');
  let current = root;
  if (!(await lstat(root)).isDirectory() || (await lstat(root)).isSymbolicLink())
    throw new Error('Root must be a real directory');
  for (let i = 0; i < pieces.length; i++) {
    current = path.join(current, pieces[i]);
    const stat = await lstat(current);
    if (stat.isSymbolicLink() || (i + 1 === pieces.length ? !stat.isFile() : !stat.isDirectory())) {
      throw new Error(`Only regular files/directories permitted: ${relative}`);
    }
  }
  return current;
}
/** @param {Buffer} bytes */
function hash(bytes) {
  return createHash('sha256').update(bytes).digest('hex');
}
/** Parse actual ELF64 little-endian program/dynamic headers, never execute ldd. @param {Buffer} b @param {string} arch */
export function neededLibraries(b, arch) {
  const fail = () => {
    throw new Error('Unsupported or malformed ELF64 payload');
  };
  /** @param {number} offset @param {number} length */
  const range = (offset, length) => {
    if (!Number.isSafeInteger(offset) || offset < 0 || length < 0 || offset + length > b.length)
      fail();
  };
  range(0, 64);
  if (
    b.subarray(0, 4).toString('hex') !== '7f454c46' ||
    b[4] !== 2 ||
    b[5] !== 1 ||
    b[6] !== 1 ||
    b.readUInt16LE(18) !== (arch === 'aarch64' ? 183 : 62) ||
    ![2, 3].includes(b.readUInt16LE(16))
  )
    fail();
  /** @param {number} offset */
  const u64 = (offset) => {
    range(offset, 8);
    const n = b.readBigUInt64LE(offset);
    if (n > BigInt(Number.MAX_SAFE_INTEGER)) fail();
    return Number(n);
  };
  const offset = u64(32),
    size = b.readUInt16LE(54),
    count = b.readUInt16LE(56);
  if (size < 56 || count === 0 || count === 65535) fail();
  range(offset, size * count);
  /** @type {{offset:number,address:number,size:number}[]} */ const loads = [];
  /** @type {{offset:number,size:number}[]} */ const dynamics = [];
  /** @type {string[]} */ const interpreters = [];
  for (let i = 0; i < count; i++) {
    const p = offset + size * i,
      start = u64(p + 8),
      length = u64(p + 32);
    range(start, length);
    if (b.readUInt32LE(p) === 3) {
      if (
        length < 2 ||
        b[start + length - 1] !== 0 ||
        b.subarray(start, start + length - 1).includes(0)
      )
        fail();
      const interpreter = b.subarray(start, start + length - 1).toString('utf8');
      if (!/^\/[A-Za-z0-9_./+-]+$/.test(interpreter) || interpreter.split('/').includes('..'))
        fail();
      interpreters.push(path.posix.basename(interpreter));
    }
    if (b.readUInt32LE(p) === 1) loads.push({ offset: start, address: u64(p + 16), size: length });
    if (b.readUInt32LE(p) === 2) dynamics.push({ offset: start, size: length });
  }
  if (dynamics.length > 1) fail();
  /** @type {number[]} */ const needed = [];
  let strtab = -1,
    strsize = -1;
  for (const dynamic of dynamics) {
    let ended = false;
    if (dynamic.size % 16) fail();
    for (let i = dynamic.offset; i < dynamic.offset + dynamic.size; i += 16) {
      const tag = u64(i),
        value = u64(i + 8);
      if (tag === 0) {
        ended = true;
        break;
      }
      if (tag === 1) needed.push(value);
      if (tag === 5) strtab = value;
      if (tag === 10) strsize = value;
    }
    if (!ended) fail();
  }
  if (!needed.length) return interpreters;
  const segment = loads.find(
    (load) => strtab >= load.address && strsize >= 0 && strtab - load.address + strsize <= load.size
  );
  if (!segment) return fail();
  const start = segment.offset + strtab - segment.address;
  return [
    ...interpreters,
    ...needed.map((index) => {
      if (index >= strsize) fail();
      const end = b.indexOf(0, start + index);
      if (end < 0 || end >= start + strsize) fail();
      const name = b.subarray(start + index, end).toString('utf8');
      if (!/^[A-Za-z0-9_.+-]+$/.test(name) || name === '.' || name === '..') fail();
      return name;
    }),
  ];
}
/** @param {Entry[]} entries @param {Map<string,Buffer>} bytes @param {string} arch */
function closure(entries, bytes, arch) {
  const paths = new Set(entries.map((e) => e.path));
  if (
    paths.size !== entries.length ||
    entries.filter((e) => e.role === 'executable').length !== 1 ||
    entries.filter((e) => e.role === 'bundle').length !== 1
  )
    throw new Error('Require unique paths, one executable and one JS/HBC bundle');
  const libraries = new Map();
  for (const entry of entries) {
    if (entry.role === 'bundle' && !/\.(js|hbc)$/.test(entry.path))
      throw new Error('Bundle must be JS or HBC');
    if (entry.role === 'license' && !bytes.get(entry.path)?.length)
      throw new Error('License material must not be empty');
    safePath(entry.path);
    if (
      !roles.has(entry.role) ||
      typeof entry.component !== 'string' ||
      !entry.component.trim() ||
      !Array.isArray(entry.requires)
    )
      throw new Error('Invalid entry metadata');
    for (const reference of entry.requires)
      if (!paths.has(reference)) throw new Error(`Missing declared closure: ${reference}`);
    if (['library', 'plugin'].includes(entry.role)) {
      const name = path.posix.basename(entry.path);
      if (libraries.has(name)) throw new Error(`Ambiguous library name: ${name}`);
      libraries.set(name, entry.path);
    }
    if (
      entry.role !== 'license' &&
      !entries.some((e) => e.role === 'license' && e.component === entry.component)
    ) {
      throw new Error(`Missing license material inventory for ${entry.component}`);
    }
  }
  for (const entry of entries.filter((e) => ['executable', 'library', 'plugin'].includes(e.role))) {
    for (const library of neededLibraries(/** @type {Buffer} */ (bytes.get(entry.path)), arch)) {
      if (!libraries.has(library))
        throw new Error(`Missing ELF dependency ${library} for ${entry.path}`);
    }
  }
}
/** @param {string} directory */
export async function verifyPackage(directory) {
  const root = path.resolve(directory);
  const manifestPath = path.join(root, 'manifest.json');
  if (!(await lstat(manifestPath)).isFile() || (await lstat(manifestPath)).isSymbolicLink())
    throw new Error('Manifest must be a regular file');
  const manifest = JSON.parse(await readFile(manifestPath, 'utf8'));
  if (
    manifest.schema !== 'rnqnx-offline-package/v1' ||
    !['aarch64', 'x86_64'].includes(manifest.arch) ||
    typeof manifest.version !== 'string' ||
    !/^[A-Za-z0-9][A-Za-z0-9._+-]{0,127}$/.test(manifest.version) ||
    !Array.isArray(manifest.files)
  )
    throw new Error('Invalid package manifest');
  /** @type {Entry[]} */ const entries = manifest.files;
  const bytes = new Map();
  for (const entry of entries) {
    const file = await regular(root, entry.path),
      content = await readFile(file),
      stat = await lstat(file);
    if (
      entry.size !== content.length ||
      entry.sha256 !== hash(content) ||
      entry.mode !== (entry.role === 'executable' ? 493 : 420) ||
      (stat.mode & 511) !== entry.mode
    )
      throw new Error(`Integrity mismatch: ${entry.path}`);
    bytes.set(entry.path, content);
  }
  /** @param {string} relative */
  async function walk(relative) {
    for (const item of await readdir(path.join(root, relative), { withFileTypes: true })) {
      const name = relative ? `${relative}/${item.name}` : item.name;
      if (item.isDirectory()) await walk(name);
      else if (!item.isFile() || (name !== 'manifest.json' && !bytes.has(name)))
        throw new Error(`Unlisted payload: ${name}`);
    }
  }
  await walk('');
  closure(entries, bytes, manifest.arch);
  return manifest;
}
/** Publish only to an exclusively created directory, manifest last. Injectable rename
 * permits deterministic I/O failure tests without touching a real target.
 * @param {string} stage @param {string} output @param {typeof rename} move */
export async function publishDirectory(stage, output, move = rename) {
  let created = false;
  try {
    await mkdir(output);
    created = true;
    const names = (await readdir(stage)).filter((name) => name !== 'manifest.json');
    for (const name of names) await move(path.join(stage, name), path.join(output, name));
    await move(path.join(stage, 'manifest.json'), path.join(output, 'manifest.json'));
    await rm(stage, { recursive: true });
  } catch (error) {
    if (created) await rm(output, { recursive: true, force: true });
    throw error;
  }
}

/** @param {string} specFile @param {string} sourceRoot @param {string} destination */
export async function createPackage(specFile, sourceRoot, destination) {
  const spec = JSON.parse(await readFile(specFile, 'utf8'));
  if (
    spec.schema !== 'rnqnx-offline-input/v1' ||
    !Array.isArray(spec.files) ||
    !['aarch64', 'x86_64'].includes(spec.arch) ||
    typeof spec.version !== 'string' ||
    !/^[A-Za-z0-9][A-Za-z0-9._+-]{0,127}$/.test(spec.version)
  )
    throw new Error('Invalid input manifest');
  const root = path.resolve(sourceRoot),
    output = path.resolve(destination);
  if (output === root || output.startsWith(root + path.sep))
    throw new Error('Output must be outside input root');
  const stage = `${output}.staging-${process.pid}`;
  // mkdir without recursive/exist_ok ensures existing payloads are never overwritten.
  await mkdir(stage);
  try {
    /** @type {Entry[]} */ const files = [];
    for (const input of spec.files) {
      const name = safePath(input.path),
        source = await regular(root, input.source);
      const content = await readFile(source),
        mode = input.role === 'executable' ? 493 : 420;
      const file = path.join(stage, name);
      await mkdir(path.dirname(file), { recursive: true });
      await writeFile(file, content, { flag: 'wx', mode });
      await chmod(file, mode);
      files.push({
        path: name,
        role: input.role,
        component: input.component,
        requires: input.requires,
        size: content.length,
        sha256: hash(content),
        mode,
      });
    }
    files.sort((a, b) => a.path.localeCompare(b.path, 'en'));
    await writeFile(
      path.join(stage, 'manifest.json'),
      JSON.stringify(
        { schema: 'rnqnx-offline-package/v1', version: spec.version, arch: spec.arch, files },
        null,
        2
      ) + '\n',
      { flag: 'wx' }
    );
    await verifyPackage(stage);
    await publishDirectory(stage, output);
    return await verifyPackage(output);
  } catch (error) {
    await rm(stage, { recursive: true, force: true });
    throw error;
  }
}
if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    const [command, ...args] = process.argv.slice(2);
    if (command === 'create' && args.length === 3) await createPackage(args[0], args[1], args[2]);
    else if (command === 'verify' && args.length === 1) await verifyPackage(args[0]);
    else
      throw new Error(
        'Usage: offline-package.mjs create INPUT.json SOURCE_ROOT OUTPUT_DIR | verify PACKAGE_DIR'
      );
    console.log(
      'OFFLINE_PACKAGE_VERIFIED (integrity and declared/ELF closure only; no target launch or license-compliance claim)'
    );
  } catch (error) {
    console.error(String(error));
    process.exitCode = 1;
  }
}
