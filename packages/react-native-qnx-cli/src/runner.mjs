import { createHash, randomBytes } from 'node:crypto';
import { createReadStream, createWriteStream } from 'node:fs';
import { lstat, mkdir, readFile, rename, rm } from 'node:fs/promises';
import { homedir } from 'node:os';
import path from 'node:path';
import { Readable } from 'node:stream';
import { pipeline } from 'node:stream/promises';
import { fileURLToPath } from 'node:url';
import { parseArgs } from 'node:util';
import { createGunzip } from 'node:zlib';

// Prebuilt development runner client. A runner is a native React Native QNX
// host built for one target and one exact SDP + Qt + React Native combination.
// The client downloads the manifest, picks the single entry that matches the
// requested target and versions, downloads that archive, and keeps it only if
// its size and SHA-256 match the manifest. ADR-0018: runners are hosted on
// this repository's GitHub Releases, and a QNX runner carries no QNX SDP
// files; the target image supplies the QNX libraries.

export const RUNNER_MANIFEST_SCHEMA = 'rnqnx-runner-manifest/v1';
export const RUNNER_TARGETS = Object.freeze([
  'qnx-aarch64-pi400',
  'macos-arm64',
  'macos-x86_64',
  'linux-x86_64',
  'linux-aarch64',
]);
const VERSION_KEYS = Object.freeze(['reactNativeQnx', 'reactNative', 'hermes', 'qt', 'sdp']);
const MAX_MANIFEST_BYTES = 1024 * 1024;
const MAX_RUNNER_BYTES = 2 * 1024 * 1024 * 1024;
const SHA256 = /^[0-9a-f]{64}$/;
const VERSION = /^[0-9A-Za-z][0-9A-Za-z.+_-]{0,63}$/;
const ID = /^[a-z0-9][a-z0-9.-]{0,127}$/;
const SONAME = /^[A-Za-z0-9_+-][A-Za-z0-9_.+-]{0,127}\.so(\.[0-9]+)*$/;
const MAX_REDIRECTS = 5;
const MAX_ARCHIVE_ENTRIES = 100000;

// File names that belong to the QNX SDP or a QNX target image. A QNX runner
// archive must not contain them: redistribution of SDP files is out of scope
// (docs/packaging.md), so the runner loads them from the developer's own
// licensed target image. Matched on the base name of every archive entry.
export const QNX_SDP_FILE_PATTERNS = Object.freeze([
  /^lib(c|c\+\+|m|gcc_s|screen|EGL|GLESv2|GLESv1_CM|slog2|socket|z|zstd|drm|fsnotify|eventfd|regex|secpol|pci|WFD|gf|iconv|cache|crypto|ssl|stdc\+\+)\.so(\.[0-9]+)*$/,
  /^ldqnx.*\.so(\.[0-9]+)*$/,
  /-mesa\.so(\.[0-9]+)*$/,
  /_dri\.so$/,
  /^qnxsdp-env\.sh$/,
]);

const help = `React Native QNX prebuilt development runner
  runner fetch --target <target> --manifest <url> [--cache <dir>]
               [--react-native-qnx <v>] [--react-native <v>] [--hermes <v>]
               [--qt <v>] [--sdp <v>|none] [--target-libs <file>]
  runner list  --manifest <url>

Targets: ${RUNNER_TARGETS.join(', ')}.
The manifest URL comes from --manifest or RNQNX_RUNNER_MANIFEST. No runner
manifest is published yet; until one is, pass the URL of your own manifest.
fetch prints the cached archive path. It verifies size and SHA-256 against the
manifest and deletes any download that does not match, then lists the
archive and refuses a QNX runner that contains QNX SDP files. Redirects are
refused, except GitHub Release downloads to githubusercontent.com. It does
not unpack, install or launch the runner, and it does not contact a device.
A QNX runner loads the QNX libraries in its manifest entry's
targetLibraries from the board. To check the board has them, run
  ls -1 /usr/lib /lib /proc/boot > target-libs.txt
on the board, copy the file back and pass --target-libs target-libs.txt.
Version flags default to the versions this checkout pins. Host runners use
--sdp none; QNX targets require --sdp <version>.
`;

/** Keys: reactNativeQnx, reactNative, hermes, qt, sdp (null for host runners). @typedef {Record<string, string | null>} RunnerVersions */
/** @typedef {{id: string, target: string, kind: string, versions: RunnerVersions, url: string, sha256: string, size: number, format: string, targetLibraries?: string[]}} RunnerEntry */
/** @typedef {{schema: string, publishedAt: string, runners: RunnerEntry[]}} RunnerManifest */

/** @param {unknown} value */
function isObject(value) {
  return Boolean(value) && typeof value === 'object' && !Array.isArray(value);
}

/**
 * Only https, plus http to loopback for local fixture servers.
 * @param {string} value @param {URL} [base]
 */
export function runnerUrl(value, base) {
  let url;
  try {
    url = new URL(value, base);
  } catch {
    throw new Error(`Invalid runner URL: ${value}`);
  }
  const loopback = ['127.0.0.1', 'localhost', '[::1]'].includes(url.hostname);
  if (url.protocol !== 'https:' && !(url.protocol === 'http:' && loopback))
    throw new Error(`Runner URLs must use https (got ${url.protocol}//${url.host}).`);
  if (url.username || url.password) throw new Error('Runner URLs must not carry credentials.');
  return url;
}

/** @param {unknown} input @returns {RunnerManifest} */
export function validateRunnerManifest(input) {
  if (!isObject(input)) throw new Error('Runner manifest must be a JSON object.');
  const manifest = /** @type {Record<string, unknown>} */ (input);
  if (manifest.schema !== RUNNER_MANIFEST_SCHEMA)
    throw new Error(`Unsupported runner manifest schema; expected ${RUNNER_MANIFEST_SCHEMA}.`);
  if (typeof manifest.publishedAt !== 'string' || Number.isNaN(Date.parse(manifest.publishedAt)))
    throw new Error('Runner manifest publishedAt must be an ISO date.');
  if (!Array.isArray(manifest.runners) || manifest.runners.length > 256)
    throw new Error('Runner manifest runners must be an array of at most 256 entries.');
  const ids = new Set();
  const runners = manifest.runners.map((raw, index) => {
    const where = `runners[${index}]`;
    if (!isObject(raw)) throw new Error(`${where} must be an object.`);
    const entry = /** @type {Record<string, any>} */ (raw);
    if (typeof entry.id !== 'string' || !ID.test(entry.id))
      throw new Error(`${where}.id is invalid.`);
    if (ids.has(entry.id)) throw new Error(`Duplicate runner id ${entry.id}.`);
    ids.add(entry.id);
    if (!RUNNER_TARGETS.includes(entry.target))
      throw new Error(`${where}.target is not supported.`);
    if (!['development', 'standalone'].includes(entry.kind))
      throw new Error(`${where}.kind must be development or standalone.`);
    if (entry.format !== 'tar.gz') throw new Error(`${where}.format must be tar.gz.`);
    if (typeof entry.sha256 !== 'string' || !SHA256.test(entry.sha256))
      throw new Error(`${where}.sha256 must be 64 lowercase hex characters.`);
    if (!Number.isInteger(entry.size) || entry.size < 1 || entry.size > MAX_RUNNER_BYTES)
      throw new Error(`${where}.size must be a positive integer within the limit.`);
    if (typeof entry.url !== 'string') throw new Error(`${where}.url must be a string.`);
    if (!isObject(entry.versions)) throw new Error(`${where}.versions must be an object.`);
    for (const key of Object.keys(entry.versions))
      if (!VERSION_KEYS.includes(key)) throw new Error(`${where}.versions.${key} is not known.`);
    for (const key of VERSION_KEYS) {
      const version = entry.versions[key];
      const hostWithoutSdp = key === 'sdp' && version === null && !entry.target.startsWith('qnx-');
      if (!hostWithoutSdp && (typeof version !== 'string' || !VERSION.test(version)))
        throw new Error(`${where}.versions.${key} is missing or invalid.`);
    }
    if (entry.target.startsWith('qnx-') && entry.versions.sdp === null)
      throw new Error(`${where} targets QNX and must name its SDP version.`);
    const libraries = entry.targetLibraries;
    if (entry.target.startsWith('qnx-') && libraries === undefined)
      throw new Error(`${where} targets QNX and must list targetLibraries.`);
    if (
      libraries !== undefined &&
      (!Array.isArray(libraries) ||
        libraries.length > 256 ||
        libraries.some((name) => typeof name !== 'string' || !SONAME.test(name)) ||
        new Set(libraries).size !== libraries.length)
    )
      throw new Error(`${where}.targetLibraries must be distinct shared-library file names.`);
    /** @type {Record<string, any>} */
    const copy = { ...entry, versions: { ...entry.versions } };
    if (libraries !== undefined) copy.targetLibraries = [...libraries];
    return /** @type {RunnerEntry} */ (copy);
  });
  return { schema: RUNNER_MANIFEST_SCHEMA, publishedAt: manifest.publishedAt, runners };
}

/**
 * Pick the one development runner for a target whose versions match exactly.
 * @param {RunnerManifest} manifest @param {string} target @param {RunnerVersions} versions
 */
export function selectRunner(manifest, target, versions) {
  if (!RUNNER_TARGETS.includes(target)) throw new Error(`Unknown runner target ${target}.`);
  const forTarget = manifest.runners.filter((r) => r.target === target && r.kind === 'development');
  const matches = forTarget.filter((r) => VERSION_KEYS.every((k) => r.versions[k] === versions[k]));
  if (matches.length === 1) return matches[0];
  if (matches.length > 1)
    throw new Error(`Manifest lists ${matches.length} identical runners for ${target}.`);
  const wanted = VERSION_KEYS.map((k) => `${k} ${versions[k] ?? 'none'}`).join(', ');
  const offered = forTarget.length
    ? forTarget
        .map(
          (r) =>
            `  ${r.id}: ${VERSION_KEYS.map((k) => `${k} ${r.versions[k] ?? 'none'}`).join(', ')}`
        )
        .join('\n')
    : '  (none for this target)';
  throw new Error(
    `No prebuilt ${target} runner for ${wanted}.\nPublished runners:\n${offered}\nBuild a custom development runner instead: rnqnx project build-native --help.`
  );
}

export const DEFAULT_IDLE_TIMEOUT_MS = 30000;

const GITHUB_RELEASE_PATH = /^\/[A-Za-z0-9_.-]+\/[A-Za-z0-9_.-]+\/releases\/(latest\/)?download\//;

/**
 * Redirects are followed only for GitHub Release downloads: from a
 * github.com release-download path to another github.com release-download
 * path (the latest/ alias) or to a githubusercontent.com storage host, all
 * over https. The archive hash in the manifest is still checked after the
 * download.
 * @param {URL} from @param {URL} to
 */
export function isAllowedRedirect(from, to) {
  if (from.protocol !== 'https:' || to.protocol !== 'https:') return false;
  if (to.username || to.password) return false;
  if (from.hostname !== 'github.com' || !GITHUB_RELEASE_PATH.test(from.pathname)) return false;
  if (to.hostname === 'github.com') return GITHUB_RELEASE_PATH.test(to.pathname);
  return to.hostname.endsWith('.githubusercontent.com');
}

/**
 * GET with redirects refused, no content encoding (so content-length is the
 * byte count) and an idle timeout that every received chunk resets.
 * @param {URL} url @param {typeof fetch} fetchImpl @param {number} idleMs
 */
async function get(url, fetchImpl, idleMs) {
  let current = url;
  for (let hop = 0; ; hop++) {
    const result = await getOnce(current, fetchImpl, idleMs);
    if (!('location' in result)) return result;
    let next;
    try {
      next = new URL(result.location, current);
    } catch {
      next = undefined;
    }
    if (!next || hop >= MAX_REDIRECTS || !isAllowedRedirect(current, next))
      throw new Error(
        `${current} redirects (${result.status}${result.location ? ` to ${result.location}` : ''}). Redirects are refused except for GitHub Release downloads; publish the final URL.`
      );
    current = next;
  }
}

/**
 * One request. A 3xx returns its location instead of a body.
 * @param {URL} url @param {typeof fetch} fetchImpl @param {number} idleMs
 * @returns {Promise<{status: number, location: string} | {response: Response, touch: () => void, done: () => void}>}
 */
async function getOnce(url, fetchImpl, idleMs) {
  const controller = new AbortController();
  /** @type {NodeJS.Timeout | undefined} */
  let timer;
  const touch = () => {
    clearTimeout(timer);
    timer = setTimeout(
      () => controller.abort(new Error(`No data from ${url.origin} for ${idleMs} ms; timed out.`)),
      idleMs
    );
    timer.unref?.();
  };
  touch();
  let response;
  try {
    response = await fetchImpl(url, {
      redirect: 'manual',
      headers: { 'accept-encoding': 'identity' },
      signal: controller.signal,
    });
  } catch (error) {
    clearTimeout(timer);
    if (controller.signal.aborted) throw controller.signal.reason;
    const cause =
      error instanceof Error && error.cause instanceof Error ? `: ${error.cause.message}` : '';
    throw new Error(`Cannot reach ${url.origin}${cause}.`);
  }
  if (response.status >= 300 && response.status < 400) {
    clearTimeout(timer);
    await response.body?.cancel().catch(() => {});
    return { status: response.status, location: response.headers.get('location') ?? '' };
  }
  if (!response.ok || !response.body) {
    clearTimeout(timer);
    throw new Error(`Download failed: ${response.status} ${response.statusText} for ${url}`);
  }
  const encoding = response.headers.get('content-encoding');
  if (encoding && encoding !== 'identity') {
    clearTimeout(timer);
    throw new Error(`${url} was sent with content-encoding ${encoding}; serve it uncompressed.`);
  }
  return { response, touch, done: () => clearTimeout(timer) };
}

/**
 * @param {string} url
 * @param {{fetchImpl?: typeof fetch, idleTimeoutMs?: number}} [options]
 */
export async function loadRunnerManifest(url, options = {}) {
  const fetchImpl = options.fetchImpl ?? fetch;
  const manifestUrl = runnerUrl(url);
  const { response, touch, done } = await get(
    manifestUrl,
    fetchImpl,
    options.idleTimeoutMs ?? DEFAULT_IDLE_TIMEOUT_MS
  );
  /** @type {Buffer[]} */
  const chunks = [];
  let received = 0;
  try {
    if (Number(response.headers.get('content-length') ?? 0) > MAX_MANIFEST_BYTES)
      throw new Error('Runner manifest is too large.');
    for await (const chunk of Readable.fromWeb(/** @type {any} */ (response.body))) {
      touch();
      received += chunk.length;
      if (received > MAX_MANIFEST_BYTES) throw new Error('Runner manifest is too large.');
      chunks.push(chunk);
    }
  } finally {
    done();
  }
  let parsed;
  try {
    parsed = JSON.parse(Buffer.concat(chunks).toString('utf8'));
  } catch {
    throw new Error('Runner manifest is not valid JSON.');
  }
  return { manifest: validateRunnerManifest(parsed), manifestUrl };
}

/** @param {string} file */
async function sha256File(file) {
  const hash = createHash('sha256');
  for await (const chunk of createReadStream(file)) hash.update(chunk);
  return hash.digest('hex');
}

/** @param {Buffer} field */
function tarString(field) {
  const end = field.indexOf(0);
  return field.subarray(0, end === -1 ? field.length : end).toString('utf8');
}

/** @param {Buffer} field */
function tarNumber(field) {
  if (field[0] & 0x80) throw new Error('Runner archive uses base-256 tar sizes; not supported.');
  const text = tarString(field).trim();
  return text ? Number.parseInt(text, 8) : 0;
}

/**
 * List the entry names of a gzip-compressed tar archive without extracting it.
 * Handles ustar prefixes, GNU long names and pax path records.
 * @param {string} file @returns {Promise<string[]>}
 */
export async function listTarGz(file) {
  /** @type {string[]} */
  const names = [];
  const source = createReadStream(file);
  const gunzip = createGunzip();
  source.on('error', (error) => gunzip.destroy(error));
  source.pipe(gunzip);
  let buffer = Buffer.alloc(0);
  let skip = 0;
  /** @type {{type: string, size: number} | undefined} */
  let meta;
  /** @type {string | undefined} */
  let nextName;
  let ended = false;
  for await (const chunk of gunzip) {
    buffer = buffer.length ? Buffer.concat([buffer, chunk]) : chunk;
    for (;;) {
      if (skip) {
        const n = Math.min(skip, buffer.length);
        buffer = buffer.subarray(n);
        skip -= n;
        if (skip) break;
      }
      if (meta) {
        const padded = Math.ceil(meta.size / 512) * 512;
        if (buffer.length < padded) break;
        const data = buffer.subarray(0, meta.size);
        buffer = buffer.subarray(padded);
        if (meta.type === 'L') nextName = tarString(data);
        else if (meta.type === 'x') {
          const match = /(?:^|\n)\d+ path=([^\n]*)\n/.exec(data.toString('utf8'));
          if (match) nextName = match[1];
        }
        meta = undefined;
        continue;
      }
      if (ended || buffer.length < 512) break;
      const header = buffer.subarray(0, 512);
      buffer = buffer.subarray(512);
      if (header.every((byte) => byte === 0)) {
        ended = true;
        break;
      }
      const type = String.fromCharCode(header[156] || 0x30);
      const size = tarNumber(header.subarray(124, 136));
      if (type === 'L' || type === 'x') {
        if (size > 64 * 1024) throw new Error('Runner archive has an oversized tar header record.');
        meta = { type, size };
        continue;
      }
      const ustar = header.subarray(257, 262).toString('latin1') === 'ustar';
      const prefix = ustar ? tarString(header.subarray(345, 500)) : '';
      const base = tarString(header.subarray(0, 100));
      const name = nextName ?? (prefix ? `${prefix}/${base}` : base);
      nextName = undefined;
      skip = Math.ceil(size / 512) * 512;
      if (type === 'g' || type === 'K') continue;
      names.push(name);
      if (names.length > MAX_ARCHIVE_ENTRIES)
        throw new Error('Runner archive has too many entries.');
    }
  }
  if (!ended) throw new Error('Runner archive is not a complete tar.gz file.');
  return names;
}

/**
 * Archive policy. Every entry is a relative path without `..`; a QNX runner
 * holds no file named like a QNX SDP or target-image file, and none of the
 * libraries its manifest entry says the board provides.
 * @param {RunnerEntry} entry @param {string[]} names
 */
export function checkRunnerContents(entry, names) {
  const unsafe = names.filter(
    (name) => !name || name.startsWith('/') || name.split('/').some((part) => part === '..')
  );
  if (unsafe.length)
    throw new Error(
      `Runner ${entry.id} has unsafe archive paths: ${unsafe.slice(0, 5).join(', ')}.`
    );
  if (!entry.target.startsWith('qnx-')) return;
  const provided = new Set(entry.targetLibraries ?? []);
  const sdp = names.filter((name) => {
    const base = name.replace(/\/+$/, '').split('/').pop() ?? '';
    return provided.has(base) || QNX_SDP_FILE_PATTERNS.some((pattern) => pattern.test(base));
  });
  if (sdp.length)
    throw new Error(
      `Runner ${entry.id} contains QNX SDP or target-image files (${sdp.slice(0, 5).join(', ')}${sdp.length > 5 ? ', ...' : ''}). Runner archives must not redistribute QNX SDP files; the board's own copies are used. See docs/adr/0018-runner-hosting-and-sdp-boundary.md.`
    );
}

/**
 * Check a board's library listing (`ls -1 /usr/lib /lib /proc/boot` output)
 * against the libraries the runner expects the target image to provide.
 * @param {RunnerEntry} entry @param {string} listing
 */
export function missingTargetLibraries(entry, listing) {
  const present = new Set(
    listing
      .split(/\r?\n/)
      .map((line) => line.trim())
      .filter((line) => line && !line.endsWith(':'))
      .map((line) => line.split('/').pop())
  );
  return (entry.targetLibraries ?? []).filter((name) => !present.has(name));
}

/** Default cache: $XDG_CACHE_HOME/rnqnx/runners or ~/.cache/rnqnx/runners. */
export function defaultRunnerCache() {
  const base = process.env.XDG_CACHE_HOME || path.join(homedir(), '.cache');
  return path.join(base, 'rnqnx', 'runners');
}

/**
 * Download (or reuse) a runner archive and verify it against the manifest.
 * @param {RunnerEntry} entry @param {URL} manifestUrl
 * @param {{cacheDir?: string, fetchImpl?: typeof fetch, idleTimeoutMs?: number}} [options]
 */
export async function fetchRunner(entry, manifestUrl, options = {}) {
  const fetchImpl = options.fetchImpl ?? fetch;
  const cacheDir = path.resolve(options.cacheDir ?? defaultRunnerCache());
  const directory = path.join(cacheDir, entry.sha256);
  const archive = path.join(directory, `${entry.id}.tar.gz`);
  await mkdir(directory, { recursive: true, mode: 0o700 });
  try {
    const stat = await lstat(archive);
    if (stat.isFile() && stat.size === entry.size && (await sha256File(archive)) === entry.sha256) {
      checkRunnerContents(entry, await listTarGz(archive));
      return { action: 'cached', archive, entry };
    }
    await rm(archive, { force: true });
  } catch (error) {
    if (!(error instanceof Error && 'code' in error && error.code === 'ENOENT')) throw error;
  }
  const url = runnerUrl(entry.url, manifestUrl);
  if (manifestUrl.protocol === 'https:' && url.protocol !== 'https:')
    throw new Error('An https manifest cannot point to a non-https runner.');
  const { response, touch, done } = await get(
    url,
    fetchImpl,
    options.idleTimeoutMs ?? DEFAULT_IDLE_TIMEOUT_MS
  );
  const declared = response.headers.get('content-length');
  if (declared !== null && Number(declared) !== entry.size) {
    done();
    throw new Error(
      `Runner ${entry.id} size ${declared} does not match the manifest (${entry.size}).`
    );
  }
  const partial = path.join(directory, `.${entry.id}.${randomBytes(6).toString('hex')}.partial`);
  const hash = createHash('sha256');
  let received = 0;
  try {
    const body = Readable.fromWeb(/** @type {any} */ (response.body));
    body.on('data', (chunk) => {
      touch();
      received += chunk.length;
      if (received > entry.size)
        body.destroy(new Error(`Runner ${entry.id} is larger than the manifest says.`));
      else hash.update(chunk);
    });
    await pipeline(body, createWriteStream(partial, { flags: 'wx', mode: 0o600 }));
    if (received !== entry.size)
      throw new Error(`Runner ${entry.id} is ${received} bytes; the manifest says ${entry.size}.`);
    const actual = hash.digest('hex');
    if (actual !== entry.sha256)
      throw new Error(
        `Runner ${entry.id} failed hash verification (got ${actual}, manifest ${entry.sha256}). The download was deleted.`
      );
    checkRunnerContents(entry, await listTarGz(partial));
    await rename(partial, archive);
  } catch (error) {
    await rm(partial, { force: true });
    throw error;
  } finally {
    done();
  }
  return { action: 'downloaded', archive, entry };
}

/**
 * Versions this checkout pins, read from its own package manifests.
 * @returns {Promise<RunnerVersions>}
 */
export async function checkoutRunnerVersions() {
  const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
  const read = async (/** @type {string} */ file) =>
    JSON.parse(await readFile(path.join(root, file), 'utf8'));
  const workspace = await read('package.json');
  const rnqnx = await read('packages/react-native-qnx/package.json');
  return {
    reactNativeQnx: rnqnx.version,
    reactNative: workspace.devDependencies['react-native'],
    hermes: null,
    qt: null,
    sdp: null,
  };
}

/** @param {string[]} argv @param {{fetchImpl?: typeof fetch}} [options] */
export async function runnerMain(argv, options = {}) {
  const { values, positionals } = parseArgs({
    args: argv,
    strict: true,
    allowPositionals: true,
    options: {
      help: { type: 'boolean', short: 'h' },
      target: { type: 'string' },
      manifest: { type: 'string' },
      cache: { type: 'string' },
      'react-native-qnx': { type: 'string' },
      'react-native': { type: 'string' },
      hermes: { type: 'string' },
      qt: { type: 'string' },
      sdp: { type: 'string' },
      'target-libs': { type: 'string' },
    },
  });
  if (values.help || !positionals.length) {
    console.log(help);
    return 0;
  }
  const [command, ...rest] = positionals;
  if (rest.length || !['fetch', 'list'].includes(command))
    throw new Error('Use runner fetch or runner list; see runner --help.');
  const manifestSource = values.manifest ?? process.env.RNQNX_RUNNER_MANIFEST;
  if (!manifestSource)
    throw new Error(
      'No runner manifest URL. No runner manifest is published yet; pass --manifest <url> or set RNQNX_RUNNER_MANIFEST, or build a custom development runner with rnqnx project build-native.'
    );
  const { manifest, manifestUrl } = await loadRunnerManifest(manifestSource, options);
  if (command === 'list') {
    console.log(
      JSON.stringify(
        manifest.runners.map(({ id, target, kind, versions }) => ({ id, target, kind, versions })),
        null,
        2
      )
    );
    return 0;
  }
  if (!values.target) throw new Error('runner fetch needs --target.');
  const pinned = await checkoutRunnerVersions();
  /** @type {Record<string, string | undefined>} */
  const flags = {
    reactNativeQnx: values['react-native-qnx'],
    reactNative: values['react-native'],
    hermes: values.hermes,
    qt: values.qt,
    sdp: values.sdp,
  };
  /** @type {any} */
  const versions = {};
  for (const key of VERSION_KEYS) {
    const flag = flags[key];
    versions[key] = flag === undefined ? pinned[key] : flag === 'none' ? null : flag;
    if (versions[key] === null && key !== 'sdp')
      throw new Error(
        `Pass --${key.replace(/[A-Z]/g, (c) => '-' + c.toLowerCase())} <version>; this checkout does not pin it.`
      );
  }
  if (values.target.startsWith('qnx-') && versions.sdp === null)
    throw new Error(
      'QNX runners are built against one QNX SDP. Pass --sdp <version>, for example --sdp 8.0.5.'
    );
  const entry = selectRunner(manifest, values.target, versions);
  const result = await fetchRunner(entry, manifestUrl, {
    cacheDir: values.cache,
    fetchImpl: options.fetchImpl,
  });
  /** @type {Record<string, unknown>} */
  const report = { ...result, installed: false, launched: false };
  if (entry.targetLibraries) {
    report.targetLibraries = entry.targetLibraries;
    if (values['target-libs'] === undefined) report.targetLibrariesChecked = false;
    else {
      const missing = missingTargetLibraries(entry, await readFile(values['target-libs'], 'utf8'));
      if (missing.length)
        throw new Error(
          `The board listing lacks libraries this runner loads from the target image: ${missing.join(', ')}. Install them from your licensed QNX SDP target image; the runner does not ship them.`
        );
      report.targetLibrariesChecked = true;
    }
  }
  console.log(JSON.stringify(report, null, 2));
  return 0;
}
