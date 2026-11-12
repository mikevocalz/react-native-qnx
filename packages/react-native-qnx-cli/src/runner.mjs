import { createHash, randomBytes } from 'node:crypto';
import { createReadStream, createWriteStream } from 'node:fs';
import { lstat, mkdir, readFile, rename, rm } from 'node:fs/promises';
import { homedir } from 'node:os';
import path from 'node:path';
import { Readable } from 'node:stream';
import { pipeline } from 'node:stream/promises';
import { fileURLToPath } from 'node:url';
import { parseArgs } from 'node:util';

// Prebuilt development runner client. A runner is a native React Native QNX
// host built for one target and one exact SDP + Qt + React Native combination.
// The client downloads the manifest, picks the single entry that matches the
// requested target and versions, downloads that archive, and keeps it only if
// its size and SHA-256 match the manifest.

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

const help = `React Native QNX prebuilt development runner
  runner fetch --target <target> --manifest <url> [--cache <dir>]
               [--react-native-qnx <v>] [--react-native <v>] [--hermes <v>]
               [--qt <v>] [--sdp <v>|none]
  runner list  --manifest <url>

Targets: ${RUNNER_TARGETS.join(', ')}.
The manifest URL comes from --manifest or RNQNX_RUNNER_MANIFEST. No runner
manifest is published yet; until one is, pass the URL of your own manifest.
fetch prints the cached archive path. It verifies size and SHA-256 against the
manifest and deletes any download that does not match. It does not unpack,
install or launch the runner, and it does not contact a device.
Version flags default to the versions this checkout pins. Host runners use
--sdp none.
`;

/** Keys: reactNativeQnx, reactNative, hermes, qt, sdp (null for host runners). @typedef {Record<string, string | null>} RunnerVersions */
/** @typedef {{id: string, target: string, kind: string, versions: RunnerVersions, url: string, sha256: string, size: number, format: string}} RunnerEntry */
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
    return /** @type {RunnerEntry} */ ({ ...entry, versions: { ...entry.versions } });
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

/** @param {URL} url @param {typeof fetch} fetchImpl */
async function get(url, fetchImpl) {
  let response;
  try {
    response = await fetchImpl(url, { redirect: 'error' });
  } catch (error) {
    const cause =
      error instanceof Error && error.cause instanceof Error ? `: ${error.cause.message}` : '';
    throw new Error(
      `Cannot reach ${url.origin}${cause}. Redirects are refused; publish the final URL.`
    );
  }
  if (!response.ok || !response.body)
    throw new Error(`Download failed: ${response.status} ${response.statusText} for ${url}`);
  return response;
}

/** @param {string} url @param {{fetchImpl?: typeof fetch}} [options] */
export async function loadRunnerManifest(url, options = {}) {
  const fetchImpl = options.fetchImpl ?? fetch;
  const manifestUrl = runnerUrl(url);
  const response = await get(manifestUrl, fetchImpl);
  const declared = Number(response.headers.get('content-length') ?? 0);
  if (declared > MAX_MANIFEST_BYTES) throw new Error('Runner manifest is too large.');
  const body = await response.text();
  if (Buffer.byteLength(body) > MAX_MANIFEST_BYTES)
    throw new Error('Runner manifest is too large.');
  let parsed;
  try {
    parsed = JSON.parse(body);
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

/** Default cache: $XDG_CACHE_HOME/rnqnx/runners or ~/.cache/rnqnx/runners. */
export function defaultRunnerCache() {
  const base = process.env.XDG_CACHE_HOME || path.join(homedir(), '.cache');
  return path.join(base, 'rnqnx', 'runners');
}

/**
 * Download (or reuse) a runner archive and verify it against the manifest.
 * @param {RunnerEntry} entry @param {URL} manifestUrl
 * @param {{cacheDir?: string, fetchImpl?: typeof fetch}} [options]
 */
export async function fetchRunner(entry, manifestUrl, options = {}) {
  const fetchImpl = options.fetchImpl ?? fetch;
  const cacheDir = path.resolve(options.cacheDir ?? defaultRunnerCache());
  const directory = path.join(cacheDir, entry.sha256);
  const archive = path.join(directory, `${entry.id}.tar.gz`);
  await mkdir(directory, { recursive: true, mode: 0o700 });
  try {
    const stat = await lstat(archive);
    if (stat.isFile() && stat.size === entry.size && (await sha256File(archive)) === entry.sha256)
      return { action: 'cached', archive, entry };
    await rm(archive, { force: true });
  } catch (error) {
    if (!(error instanceof Error && 'code' in error && error.code === 'ENOENT')) throw error;
  }
  const url = runnerUrl(entry.url, manifestUrl);
  if (manifestUrl.protocol === 'https:' && url.protocol !== 'https:')
    throw new Error('An https manifest cannot point to a non-https runner.');
  const response = await get(url, fetchImpl);
  const declared = response.headers.get('content-length');
  if (declared !== null && Number(declared) !== entry.size)
    throw new Error(
      `Runner ${entry.id} size ${declared} does not match the manifest (${entry.size}).`
    );
  const partial = path.join(directory, `.${entry.id}.${randomBytes(6).toString('hex')}.partial`);
  const hash = createHash('sha256');
  let received = 0;
  try {
    const body = Readable.fromWeb(/** @type {any} */ (response.body));
    body.on('data', (chunk) => {
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
    await rename(partial, archive);
  } catch (error) {
    await rm(partial, { force: true });
    throw error;
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
  const entry = selectRunner(manifest, values.target, versions);
  const result = await fetchRunner(entry, manifestUrl, {
    cacheDir: values.cache,
    fetchImpl: options.fetchImpl,
  });
  console.log(JSON.stringify({ ...result, installed: false, launched: false }, null, 2));
  return 0;
}
