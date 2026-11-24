import { createHash } from 'node:crypto';
import { lstat, mkdtemp, readdir, readFile, rm, writeFile, mkdir } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { gzipSync } from 'node:zlib';
import {
  checkRunnerContents,
  listTarGz,
  validateRunnerManifest,
  RUNNER_MANIFEST_SCHEMA,
} from './runner.mjs';
import {
  RUNTIME_FILE,
  RUNTIME_SCHEMA,
  readHermesBytecodeVersion,
  validateRuntimeInfo,
} from './runtime-compat.mjs';

// `rnqnx runner pack`: turn a staged runner directory into the archive and the
// manifest entry that `runner fetch` consumes (ADR-0018). Nothing is
// published. The output is reproducible: tar entries are sorted, owner and time
// fields are zero, gzip carries no timestamp, and every JSON file is written
// with sorted keys. The one time value, the manifest's publishedAt, is an
// input (`--published-at` or SOURCE_DATE_EPOCH) and is never read from the
// clock, so the same stage and inputs give the same bytes and SHA-256. The
// Hermes bytecode version is measured by compiling an empty file with the
// runner's own hermesc and reading the header, not typed in.

/** @param {string} name @param {number} size @param {string} type @param {number} mode */
function tarHeader(name, size, type, mode) {
  const h = Buffer.alloc(512);
  Buffer.from(name).subarray(0, 100).copy(h, 0);
  h.write(mode.toString(8).padStart(7, '0') + '\0', 100);
  h.write('0000000\0', 108);
  h.write('0000000\0', 116);
  h.write(size.toString(8).padStart(11, '0') + '\0', 124);
  h.write('00000000000\0', 136);
  h.write(type, 156);
  h.write('ustar\0', 257);
  h.write('00', 263);
  h.fill(0x20, 148, 156);
  let sum = 0;
  for (const byte of h) sum += byte;
  h.write(sum.toString(8).padStart(6, '0') + '\0 ', 148);
  return h;
}

/**
 * Recursively sort object keys so the serialized form does not depend on the
 * order the caller built the value in. Arrays keep their order.
 * @param {unknown} value @returns {unknown}
 */
export function sortKeys(value) {
  if (Array.isArray(value)) return value.map(sortKeys);
  if (value && typeof value === 'object')
    return Object.fromEntries(
      Object.keys(value)
        .sort()
        .map((key) => [key, sortKeys(/** @type {Record<string, unknown>} */ (value)[key])])
    );
  return value;
}

/** Canonical JSON text: sorted keys, two-space indent, one trailing newline. */
export const canonicalJson = (/** @type {unknown} */ value) =>
  JSON.stringify(sortKeys(value), null, 2) + '\n';

/**
 * The manifest timestamp, taken from an explicit ISO date or from a
 * SOURCE_DATE_EPOCH value (seconds since the epoch), never from the clock.
 * @param {string | undefined} publishedAt @param {string | undefined} epoch
 */
export function resolvePublishedAt(publishedAt, epoch) {
  if (publishedAt !== undefined) {
    if (Number.isNaN(Date.parse(publishedAt)))
      throw new Error(`--published-at ${JSON.stringify(publishedAt)} is not an ISO date.`);
    return publishedAt;
  }
  if (epoch !== undefined && epoch !== '') {
    if (!/^[0-9]{1,12}$/.test(epoch))
      throw new Error(`SOURCE_DATE_EPOCH ${JSON.stringify(epoch)} is not whole seconds.`);
    return new Date(Number(epoch) * 1000).toISOString();
  }
  throw new Error(
    'runner pack needs a fixed time so the output is reproducible: pass --published-at <ISO date> or set SOURCE_DATE_EPOCH.'
  );
}

/** @param {Buffer} body */
const padded = (body) => Buffer.concat([body, Buffer.alloc((512 - (body.length % 512)) % 512)]);

/** @param {string} root @param {string} [relative] @returns {Promise<string[]>} */
async function listFiles(root, relative = '') {
  /** @type {string[]} */
  const files = [];
  const entries = await readdir(path.join(root, relative), { withFileTypes: true });
  for (const entry of entries.sort((a, b) => (a.name < b.name ? -1 : a.name > b.name ? 1 : 0))) {
    const next = relative ? `${relative}/${entry.name}` : entry.name;
    if (entry.isSymbolicLink())
      throw new Error(`${next} is a symlink; runner archives hold regular files only.`);
    if (entry.isDirectory()) files.push(...(await listFiles(root, next)));
    else if (entry.isFile()) files.push(next);
    else throw new Error(`${next} is not a regular file or directory.`);
  }
  return files;
}

/**
 * Measure the bytecode version the runner's compiler writes.
 * @param {string} hermesc
 */
export async function measureBytecodeVersion(hermesc) {
  const directory = await mkdtemp(path.join(tmpdir(), 'rnqnx-hbc-'));
  try {
    const source = path.join(directory, 'empty.js');
    const output = path.join(directory, 'empty.hbc');
    await writeFile(source, '');
    await new Promise((resolve, reject) => {
      const child = spawn(hermesc, ['-emit-binary', '-out', output, source], {
        stdio: ['ignore', 'ignore', 'pipe'],
        shell: false,
      });
      let stderr = '';
      child.stderr.on('data', (chunk) => (stderr += chunk));
      child.once('error', reject);
      child.once('exit', (code) =>
        code === 0 ? resolve(undefined) : reject(new Error(`hermesc exited ${code}: ${stderr}`))
      );
    });
    const version = await readHermesBytecodeVersion(output);
    if (version === null) throw new Error('hermesc did not write Hermes bytecode.');
    return version;
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
}

/**
 * @param {{stage: string, out: string, id: string, target: string,
 *   versions: Record<string, string | null>, hermesc: string, nitroAbi: string,
 *   builtins: unknown, targetLibraries?: string[], publishedAt: string}} options
 */
export async function packRunner(options) {
  const stage = path.resolve(options.stage);
  const publishedAt = resolvePublishedAt(options.publishedAt, undefined);
  const names = await listFiles(stage).catch((error) => {
    if (error?.code === 'ENOENT' || error?.code === 'ENOTDIR')
      throw new Error(`The stage ${stage} is not a readable directory.`);
    throw error;
  });
  if (names.length === 0) throw new Error(`The stage ${stage} holds no files.`);
  if (names.includes(RUNTIME_FILE))
    throw new Error(`The stage already contains ${RUNTIME_FILE}; pack writes it.`);
  const hermesBytecodeVersion = await measureBytecodeVersion(options.hermesc);
  const runtime = validateRuntimeInfo(
    { hermesBytecodeVersion, nitroAbi: options.nitroAbi, builtins: options.builtins },
    'runtime'
  );
  const descriptor = {
    schema: RUNTIME_SCHEMA,
    runnerId: options.id,
    target: options.target,
    versions: options.versions,
    ...runtime,
  };
  const descriptorBody = Buffer.from(canonicalJson(descriptor));

  /** @type {Buffer[]} */
  const parts = [];
  const all = [...names, RUNTIME_FILE].sort();
  for (const name of all) {
    const body =
      name === RUNTIME_FILE ? descriptorBody : await readFile(path.join(stage, ...name.split('/')));
    if (Buffer.byteLength(name) > 100) {
      const long = Buffer.from(name + '\0');
      parts.push(tarHeader('././@LongLink', long.length, 'L', 0o644), padded(long));
    }
    const mode = name === RUNTIME_FILE ? 0o644 : (await lstat(path.join(stage, name))).mode & 0o755;
    parts.push(tarHeader(name, body.length, '0', mode), padded(body));
  }
  parts.push(Buffer.alloc(1024));
  const archive = gzipSync(Buffer.concat(parts), { level: 9 });
  const sha256 = createHash('sha256').update(archive).digest('hex');

  const entry = {
    id: options.id,
    target: options.target,
    kind: 'development',
    versions: options.versions,
    url: `${options.id}.tar.gz`,
    sha256,
    size: archive.length,
    format: 'tar.gz',
    ...(options.targetLibraries ? { targetLibraries: [...options.targetLibraries].sort() } : {}),
    runtime,
  };
  const manifest = validateRunnerManifest({
    schema: RUNNER_MANIFEST_SCHEMA,
    publishedAt,
    runners: [entry],
  });
  checkRunnerContents(manifest.runners[0], all);

  const out = path.resolve(options.out);
  await mkdir(out, { recursive: true });
  const archivePath = path.join(out, `${options.id}.tar.gz`);
  const manifestPath = path.join(out, 'rnqnx-runner-manifest.json');
  const descriptorPath = path.join(out, RUNTIME_FILE);
  for (const target of [archivePath, manifestPath, descriptorPath])
    if (
      await lstat(target).then(
        () => true,
        () => false
      )
    )
      throw new Error(`EEXIST: ${target} already exists; pack never overwrites.`);
  await writeFile(archivePath, archive, { flag: 'wx' });
  await writeFile(manifestPath, canonicalJson(manifest), { flag: 'wx' });
  // The same bytes that sit inside the archive, for readers who do not unpack it.
  await writeFile(descriptorPath, descriptorBody, { flag: 'wx' });
  // Prove the written archive reads back with the entries recorded above.
  const listed = await listTarGz(archivePath);
  if (listed.length !== all.length)
    throw new Error('Packed archive does not list back as written.');
  return {
    archive: archivePath,
    manifest: manifestPath,
    descriptor: descriptorPath,
    entry,
    entries: all.length,
  };
}
