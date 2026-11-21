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
// published. The archive is reproducible: entries are sorted, owner and time
// fields are zero, and gzip carries no timestamp, so the same stage gives the
// same SHA-256. The Hermes bytecode version is measured by compiling an empty
// file with the runner's own hermesc and reading the header, not typed in.

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
 *   builtins: unknown, targetLibraries?: string[], publishedAt?: string}} options
 */
export async function packRunner(options) {
  const stage = path.resolve(options.stage);
  const names = await listFiles(stage);
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
  const descriptorBody = Buffer.from(JSON.stringify(descriptor, null, 2) + '\n');

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
    ...(options.targetLibraries ? { targetLibraries: options.targetLibraries } : {}),
    runtime,
  };
  const manifest = validateRunnerManifest({
    schema: RUNNER_MANIFEST_SCHEMA,
    publishedAt: options.publishedAt ?? new Date().toISOString(),
    runners: [entry],
  });
  checkRunnerContents(manifest.runners[0], all);

  const out = path.resolve(options.out);
  await mkdir(out, { recursive: true });
  const archivePath = path.join(out, `${options.id}.tar.gz`);
  const manifestPath = path.join(out, 'rnqnx-runner-manifest.json');
  for (const target of [archivePath, manifestPath])
    if (
      await lstat(target).then(
        () => true,
        () => false
      )
    )
      throw new Error(`EEXIST: ${target} already exists; pack never overwrites.`);
  await writeFile(archivePath, archive, { flag: 'wx' });
  await writeFile(manifestPath, JSON.stringify(manifest, null, 2) + '\n', { flag: 'wx' });
  // Prove the written archive reads back with the entries recorded above.
  const listed = await listTarGz(archivePath);
  if (listed.length !== all.length)
    throw new Error('Packed archive does not list back as written.');
  return {
    archive: archivePath,
    manifest: manifestPath,
    entry,
    entries: all.length,
  };
}
