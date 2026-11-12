import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFile, readdir, mkdtemp, rm, writeFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { tmpdir } from 'node:os';
import path from 'node:path';
import {
  fetchRunner,
  loadRunnerManifest,
  runnerMain,
  runnerUrl,
  selectRunner,
  validateRunnerManifest,
} from '../../packages/react-native-qnx-cli/src/runner.mjs';

// Local fixture server standing in for the publication host. The archives are
// synthetic bytes; nothing here is a real runner binary.
const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-runner-'));
const good = Buffer.from('synthetic runner archive v1\n'.repeat(1000));
const sha = (/** @type {Buffer} */ b) => createHash('sha256').update(b).digest('hex');
/** @type {Map<string, Buffer | (() => [number, Buffer])>} */
const routes = new Map();
const hits = new Map();
/** Raw handlers for misbehaving-server cases. @type {Map<string, (req: import('node:http').IncomingMessage, res: import('node:http').ServerResponse) => void>} */
const handlers = new Map();
/** @type {Map<string, string | undefined>} */
const acceptEncoding = new Map();
const server = createServer((request, response) => {
  const url = request.url ?? '';
  hits.set(url, (hits.get(url) ?? 0) + 1);
  acceptEncoding.set(url, request.headers['accept-encoding']);
  const handler = handlers.get(url);
  if (handler) return handler(request, response);
  const route = routes.get(url);
  if (!route) return response.writeHead(404).end();
  if (typeof route === 'function') {
    const [status, body] = route();
    return response.writeHead(status).end(body);
  }
  response.writeHead(200, { 'content-length': route.length }).end(route);
});
await new Promise((resolve) => server.listen(0, '127.0.0.1', () => resolve(undefined)));
const address = server.address();
assert.ok(address && typeof address === 'object');
const base = `http://127.0.0.1:${address.port}`;
let checks = 0;
const ok = (/** @type {unknown} */ v, /** @type {string} */ m) => {
  assert.ok(v, m);
  checks++;
};
const rejects = async (/** @type {Promise<unknown>} */ p, /** @type {RegExp} */ re) => {
  await assert.rejects(p, re);
  checks++;
};

const versions = {
  reactNativeQnx: '0.1.0',
  reactNative: '0.88.1',
  hermes: '0.88.1',
  qt: '6.12.0',
  sdp: '8.0.5',
};
const hostVersions = { ...versions, sdp: null };
const entry = (/** @type {Record<string, unknown>} */ over) => ({
  id: 'qnx-pi400-dev',
  target: 'qnx-aarch64-pi400',
  kind: 'development',
  versions,
  url: 'runners/qnx-pi400-dev.tar.gz',
  sha256: sha(good),
  size: good.length,
  format: 'tar.gz',
  ...over,
});
const manifest = {
  schema: 'rnqnx-runner-manifest/v1',
  publishedAt: '2026-11-12T00:00:00Z',
  runners: [
    entry({}),
    entry({
      id: 'macos-dev',
      target: 'macos-arm64',
      versions: hostVersions,
      url: 'runners/macos.tar.gz',
    }),
    entry({
      id: 'linux-dev',
      target: 'linux-x86_64',
      versions: hostVersions,
      url: 'runners/linux.tar.gz',
      sha256: sha(Buffer.from('other')),
    }),
    entry({
      id: 'qnx-pi400-old',
      versions: { ...versions, qt: '6.11.0' },
      url: 'runners/old.tar.gz',
    }),
  ],
};
routes.set('/manifest.json', Buffer.from(JSON.stringify(manifest)));
routes.set('/runners/qnx-pi400-dev.tar.gz', good);
routes.set('/runners/macos.tar.gz', good);
routes.set('/runners/linux.tar.gz', good); // hash in manifest does not match
routes.set('/bad-schema.json', Buffer.from(JSON.stringify({ ...manifest, schema: 'v0' })));
routes.set('/not-json', Buffer.from('<html>'));
const cacheDir = path.join(temporary, 'cache');
const quiet = async (/** @type {() => Promise<unknown>} */ run) => {
  const log = console.log;
  /** @type {string[]} */
  const lines = [];
  console.log = (/** @type {string} */ line) => lines.push(line);
  try {
    await run();
  } finally {
    console.log = log;
  }
  return lines.join('\n');
};

try {
  // URL policy.
  ok(runnerUrl('https://example.com/m.json').protocol === 'https:', 'https allowed');
  ok(runnerUrl('http://127.0.0.1:1/m.json').protocol === 'http:', 'loopback http allowed');
  for (const bad of [
    'http://example.com/m.json',
    'file:///etc/passwd',
    'ftp://x/y',
    'https://u:p@x/y',
    'nope',
  ])
    (assert.throws(() => runnerUrl(bad), /runner URL|https|credentials/i), checks++);

  // Manifest validation.
  const loaded = await loadRunnerManifest(`${base}/manifest.json`);
  ok(loaded.manifest.runners.length === 4, 'manifest loads');
  await rejects(loadRunnerManifest(`${base}/bad-schema.json`), /schema/);
  await rejects(loadRunnerManifest(`${base}/not-json`), /valid JSON/);
  await rejects(loadRunnerManifest(`${base}/missing.json`), /404/);
  for (const bad of [
    { runners: [entry({ sha256: 'ABC' })] },
    { runners: [entry({ size: 0 })] },
    { runners: [entry({ target: 'windows-x64' })] },
    { runners: [entry({ format: 'zip' })] },
    { runners: [entry({ versions: { ...versions, sdp: null } })] },
    { runners: [entry({ versions: { ...versions, extra: '1' } })] },
    { runners: [entry({}), entry({})] },
    { publishedAt: 'yesterday' },
  ]) {
    assert.throws(() => validateRunnerManifest({ ...manifest, ...bad }));
    checks++;
  }

  // Selection is exact on target and every version.
  ok(
    selectRunner(loaded.manifest, 'qnx-aarch64-pi400', versions).id === 'qnx-pi400-dev',
    'exact match'
  );
  assert.throws(
    () =>
      selectRunner(loaded.manifest, 'qnx-aarch64-pi400', { ...versions, reactNative: '0.89.0' }),
    (error) =>
      error instanceof Error &&
      /No prebuilt/.test(error.message) &&
      /qnx-pi400-old/.test(error.message) &&
      /build-native/.test(error.message)
  );
  checks++;
  assert.throws(
    () => selectRunner(loaded.manifest, 'linux-aarch64', hostVersions),
    /none for this target/
  );
  checks++;

  // Download, verify, cache.
  const first = await fetchRunner(
    selectRunner(loaded.manifest, 'qnx-aarch64-pi400', versions),
    loaded.manifestUrl,
    { cacheDir }
  );
  ok(first.action === 'downloaded', 'first fetch downloads');
  ok(sha(await readFile(first.archive)) === sha(good), 'archive bytes verified');
  const second = await fetchRunner(
    selectRunner(loaded.manifest, 'qnx-aarch64-pi400', versions),
    loaded.manifestUrl,
    { cacheDir }
  );
  ok(
    second.action === 'cached' && hits.get('/runners/qnx-pi400-dev.tar.gz') === 1,
    'second fetch reuses cache'
  );
  await writeFile(first.archive, 'tampered');
  const third = await fetchRunner(
    selectRunner(loaded.manifest, 'qnx-aarch64-pi400', versions),
    loaded.manifestUrl,
    { cacheDir }
  );
  ok(
    third.action === 'downloaded' && hits.get('/runners/qnx-pi400-dev.tar.gz') === 2,
    'tampered cache is replaced'
  );

  // Hash mismatch: error, nothing kept.
  const linux = selectRunner(loaded.manifest, 'linux-x86_64', hostVersions);
  await rejects(fetchRunner(linux, loaded.manifestUrl, { cacheDir }), /failed hash verification/);
  ok(
    (await readdir(path.join(cacheDir, linux.sha256))).length === 0,
    'mismatched download deleted'
  );

  // Size mismatch from the server.
  routes.set('/runners/short.tar.gz', good.subarray(0, 10));
  await rejects(
    fetchRunner(entry({ id: 'short', url: 'runners/short.tar.gz' }), loaded.manifestUrl, {
      cacheDir,
    }),
    /size/
  );
  // https manifest cannot hand out an http runner.
  await rejects(
    fetchRunner(
      entry({ id: 'down', url: 'http://127.0.0.1:1/x' }),
      new URL('https://example.com/m.json'),
      { cacheDir }
    ),
    /non-https/
  );

  // Same-size tamper: only the hash check catches it.
  const cachedArchive = third.archive;
  await writeFile(cachedArchive, Buffer.alloc(good.length, 0x78));
  const beforeHits = hits.get('/runners/qnx-pi400-dev.tar.gz');
  const fourth = await fetchRunner(
    selectRunner(loaded.manifest, 'qnx-aarch64-pi400', versions),
    loaded.manifestUrl,
    { cacheDir }
  );
  ok(
    fourth.action === 'downloaded' &&
      hits.get('/runners/qnx-pi400-dev.tar.gz') === (beforeHits ?? 0) + 1 &&
      sha(await readFile(fourth.archive)) === sha(good),
    'same-size tampered cache is re-downloaded'
  );
  ok(
    acceptEncoding.get('/runners/qnx-pi400-dev.tar.gz') === 'identity',
    'asks for identity encoding'
  );

  // Redirects are refused, for the manifest and for archives.
  handlers.set('/moved.json', (_req, res) =>
    res.writeHead(302, { location: '/manifest.json' }).end()
  );
  handlers.set('/runners/moved.tar.gz', (_req, res) =>
    res.writeHead(301, { location: '/runners/qnx-pi400-dev.tar.gz' }).end()
  );
  await rejects(loadRunnerManifest(`${base}/moved.json`), /redirects \(302.*publish the final URL/);
  await rejects(
    fetchRunner(entry({ id: 'moved', url: 'runners/moved.tar.gz' }), loaded.manifestUrl, {
      cacheDir,
    }),
    /redirects \(301/
  );

  // Byte counting: short and long bodies without content-length, and a
  // content-length that promises more than is sent.
  const chunked =
    (/** @type {Buffer} */ body) => (/** @type {any} */ _req, /** @type {any} */ res) => {
      res.writeHead(200, { 'transfer-encoding': 'chunked' });
      res.end(body);
    };
  handlers.set('/runners/short-chunked.tar.gz', chunked(good.subarray(0, 100)));
  handlers.set(
    '/runners/long-chunked.tar.gz',
    chunked(Buffer.concat([good, Buffer.from('extra')]))
  );
  handlers.set('/runners/lying.tar.gz', (_req, res) => {
    res.writeHead(200, { 'content-length': good.length });
    res.write(good.subarray(0, 100));
    res.destroy();
  });
  for (const [id, pattern] of [
    ['short-chunked', /is 100 bytes; the manifest says/],
    ['long-chunked', /larger than the manifest/],
    ['lying', /terminated|socket|closed|aborted|bytes/i],
  ]) {
    const e = entry({ id: String(id), url: `runners/${id}.tar.gz` });
    await rejects(
      fetchRunner(e, loaded.manifestUrl, { cacheDir }),
      /** @type {RegExp} */ (pattern)
    );
    ok(
      (await readdir(path.join(cacheDir, e.sha256))).filter((f) => f.includes(String(id)))
        .length === 0,
      `${id} left nothing`
    );
  }
  handlers.set('/huge.json', (_req, res) => {
    res.writeHead(200, { 'transfer-encoding': 'chunked' });
    res.end(Buffer.alloc(1024 * 1024 + 1, 0x20));
  });
  await rejects(loadRunnerManifest(`${base}/huge.json`), /too large/);
  handlers.set('/gzip.tar.gz', (_req, res) =>
    res.writeHead(200, { 'content-encoding': 'gzip', 'content-length': good.length }).end(good)
  );
  await rejects(
    fetchRunner(entry({ id: 'gzip', url: 'gzip.tar.gz' }), loaded.manifestUrl, { cacheDir }),
    /content-encoding gzip/
  );

  // A server that stops sending is abandoned after the idle timeout.
  handlers.set('/runners/stall.tar.gz', (_req, res) => {
    res.writeHead(200, { 'content-length': good.length });
    res.write(good.subarray(0, 10));
  });
  await rejects(
    fetchRunner(entry({ id: 'stall', url: 'runners/stall.tar.gz' }), loaded.manifestUrl, {
      cacheDir,
      idleTimeoutMs: 200,
    }),
    /timed out/
  );

  // CLI.
  const out = await quiet(() =>
    runnerMain([
      'fetch',
      '--target',
      'macos-arm64',
      '--manifest',
      `${base}/manifest.json`,
      '--cache',
      cacheDir,
      '--react-native-qnx',
      '0.1.0',
      '--react-native',
      '0.88.1',
      '--hermes',
      '0.88.1',
      '--qt',
      '6.12.0',
      '--sdp',
      'none',
    ])
  );
  const parsed = JSON.parse(out);
  ok(
    parsed.action === 'downloaded' && parsed.installed === false && parsed.launched === false,
    'CLI fetch reports scope'
  );
  const saved = process.env.RNQNX_RUNNER_MANIFEST;
  delete process.env.RNQNX_RUNNER_MANIFEST;
  await rejects(
    runnerMain(['fetch', '--target', 'macos-arm64']),
    /No runner manifest is published yet/
  );
  if (saved !== undefined) process.env.RNQNX_RUNNER_MANIFEST = saved;
  await rejects(
    runnerMain([
      'fetch',
      '--target',
      'qnx-aarch64-pi400',
      '--manifest',
      `${base}/manifest.json`,
      '--cache',
      cacheDir,
      '--react-native-qnx',
      '0.1.0',
      '--react-native',
      '0.88.1',
      '--hermes',
      '0.88.1',
      '--qt',
      '6.12.0',
    ]),
    /QNX runners are built against one QNX SDP\. Pass --sdp <version>/
  );
  await rejects(
    runnerMain(['fetch', '--target', 'macos-arm64', '--manifest', `${base}/manifest.json`]),
    /does not pin/
  );
  const list = JSON.parse(
    await quiet(() => runnerMain(['list', '--manifest', `${base}/manifest.json`]))
  );
  ok(list.length === 4 && !('sha256' in list[0]), 'list prints entries');
  console.log(
    `Runner manifest client: ${checks} checks passed (local fixture server, synthetic archives).`
  );
} finally {
  server.close();
  await rm(temporary, { recursive: true, force: true });
}
