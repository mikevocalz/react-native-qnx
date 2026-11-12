import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFile, readdir, mkdtemp, rm, writeFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { gzipSync } from 'node:zlib';
import {
  checkRunnerContents,
  fetchRunner,
  isAllowedRedirect,
  listTarGz,
  loadRunnerManifest,
  missingTargetLibraries,
  runnerMain,
  runnerUrl,
  selectRunner,
  validateRunnerManifest,
} from '../../packages/react-native-qnx-cli/src/runner.mjs';

// Local fixture server standing in for the publication host. The archives are
// synthetic bytes; nothing here is a real runner binary.
const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-runner-'));

/**
 * Minimal ustar writer for fixtures. Each file is [name, body] or
 * [name, body, typeflag]; names over 100 bytes use a GNU long-name record.
 * @param {Array<[string, string] | [string, string, string]>} files
 */
const tarGz = (files) => {
  /** @param {string} name @param {number} size @param {string} type */
  const header = (name, size, type) => {
    const h = Buffer.alloc(512);
    h.write(name.slice(0, 100), 0, 'utf8');
    h.write('0000644\0', 100);
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
  };
  /** @param {Buffer} body */
  const padded = (body) => Buffer.concat([body, Buffer.alloc((512 - (body.length % 512)) % 512)]);
  /** @type {Buffer[]} */
  const parts = [];
  for (const [name, body, type = '0'] of files) {
    if (Buffer.byteLength(name) > 100) {
      const long = Buffer.from(name + '\0');
      parts.push(header('././@LongLink', long.length, 'L'), padded(long));
    }
    const data = Buffer.from(body);
    parts.push(header(name, data.length, type), padded(data));
  }
  parts.push(Buffer.alloc(1024));
  return gzipSync(Buffer.concat(parts));
};
const good = tarGz([
  ['bin/rnqnx-dev-runner', 'synthetic runner binary\n'.repeat(200)],
  ['lib/libQt6Core.so.6', 'synthetic Qt library\n'],
  ['lib/libhermesvmlean.so', 'synthetic Hermes library\n'],
  ['licenses/LGPL-3.0-only.txt', 'synthetic notice\n'],
]);
const targetLibraries = ['libc.so.6', 'libc++.so.2', 'libscreen.so.1', 'libEGL.so.1'];
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
  targetLibraries,
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
      targetLibraries: undefined,
    }),
    entry({
      id: 'linux-dev',
      target: 'linux-x86_64',
      versions: hostVersions,
      url: 'runners/linux.tar.gz',
      targetLibraries: undefined,
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
    { runners: [entry({ targetLibraries: undefined })] },
    { runners: [entry({ targetLibraries: ['libc.so.6', 'libc.so.6'] })] },
    { runners: [entry({ targetLibraries: ['../libc.so.6'] })] },
    { runners: [entry({ targetLibraries: 'libc.so.6' })] },
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

  // GitHub Release downloads may redirect to githubusercontent.com storage.
  const release = new URL(
    'https://github.com/mikevocalz/react-native-qnx/releases/download/runner-qnx-pi400-dev-0.1.0/a.tar.gz'
  );
  ok(
    isAllowedRedirect(
      release,
      new URL('https://release-assets.githubusercontent.com/github-production-release-asset/1/x')
    ),
    'release asset to storage allowed'
  );
  ok(
    isAllowedRedirect(
      new URL('https://github.com/o/r/releases/latest/download/m.json'),
      new URL('https://github.com/o/r/releases/download/v1/m.json')
    ),
    'latest alias allowed'
  );
  for (const [from, to] of [
    [release.href, 'http://objects.githubusercontent.com/x'],
    [release.href, 'https://example.com/x'],
    [release.href, 'https://github.com/o/r/archive/main.tar.gz'],
    [release.href, 'https://evilgithubusercontent.com/x'],
    ['https://github.com/o/r/blob/main/x', 'https://objects.githubusercontent.com/x'],
    ['https://example.com/x', 'https://objects.githubusercontent.com/x'],
  ]) {
    ok(!isAllowedRedirect(new URL(from), new URL(to)), `redirect ${from} -> ${to} refused`);
  }
  // The loop follows an allowed chain and refuses a bad hop, via a fake fetch.
  const manifestBody = Buffer.from(JSON.stringify(manifest));
  /** @type {string[]} */
  const visited = [];
  /** @param {Record<string, [number, string | Buffer]>} table @returns {typeof fetch} */
  const fakeFetch = (table) =>
    /** @type {any} */ (
      async (/** @type {URL} */ url) => {
        visited.push(url.href);
        const [status, value] = table[url.href] ?? [404, ''];
        if (status >= 300 && status < 400)
          return new Response(null, { status, headers: { location: String(value) } });
        return new Response(value, { status, headers: { 'content-length': String(value.length) } });
      }
    );
  const storage = 'https://release-assets.githubusercontent.com/m.json';
  const viaGithub = await loadRunnerManifest(
    'https://github.com/o/r/releases/latest/download/m.json',
    {
      fetchImpl: fakeFetch({
        'https://github.com/o/r/releases/latest/download/m.json': [
          302,
          'https://github.com/o/r/releases/download/v1/m.json',
        ],
        'https://github.com/o/r/releases/download/v1/m.json': [302, storage],
        [storage]: [200, manifestBody],
      }),
    }
  );
  ok(
    viaGithub.manifest.runners.length === 4 && visited.length === 3,
    'GitHub release redirect chain followed'
  );
  await rejects(
    loadRunnerManifest('https://github.com/o/r/releases/download/v1/m.json', {
      fetchImpl: fakeFetch({
        'https://github.com/o/r/releases/download/v1/m.json': [302, 'https://example.com/m.json'],
      }),
    }),
    /redirects \(302 to https:\/\/example\.com/
  );
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

  // Archive listing and the SDP boundary.
  const qnxEntry = entry({});
  const listed = await listTarGz(first.archive);
  ok(
    listed.length === 4 && listed.includes('lib/libhermesvmlean.so'),
    'archive listing reads every entry'
  );
  const longName = `share/${'x'.repeat(120)}/notice.txt`;
  const longFile = path.join(temporary, 'long.tar.gz');
  await writeFile(longFile, tarGz([[longName, 'n']]));
  ok((await listTarGz(longFile))[0] === longName, 'GNU long names are read');
  await writeFile(path.join(temporary, 'truncated.tar.gz'), gzipSync(Buffer.alloc(100)));
  await rejects(listTarGz(path.join(temporary, 'truncated.tar.gz')), /not a complete tar/);
  for (const bad of [
    'lib/libc.so.6',
    'lib/libc++.so.2',
    'usr/lib/libscreen.so.1',
    'lib/libEGL.so.1',
    'graphics/libGLESv2-mesa.so',
    'graphics/v3d_dri.so',
    'lib/libslog2.so.1',
  ]) {
    assert.throws(
      () => checkRunnerContents(qnxEntry, ['bin/rnqnx-dev-runner', bad]),
      /must not redistribute QNX SDP files/
    );
    checks++;
  }
  assert.throws(() => checkRunnerContents(qnxEntry, ['../etc/passwd']), /unsafe archive paths/);
  checks++;
  checkRunnerContents(qnxEntry, listed);
  checkRunnerContents(entry({ target: 'macos-arm64', versions: hostVersions }), ['lib/libz.so.1']);
  checks++;
  // A QNX runner archive that carries an SDP library is refused after download.
  const withSdp = tarGz([
    ['bin/rnqnx-dev-runner', 'x'],
    ['lib/libscreen.so.1', 'not ours to ship'],
  ]);
  routes.set('/runners/with-sdp.tar.gz', withSdp);
  const sdpEntry = entry({
    id: 'with-sdp',
    url: 'runners/with-sdp.tar.gz',
    sha256: sha(withSdp),
    size: withSdp.length,
  });
  await rejects(
    fetchRunner(sdpEntry, loaded.manifestUrl, { cacheDir }),
    /libscreen\.so\.1.*must not redistribute/
  );
  ok(
    (await readdir(path.join(cacheDir, sdpEntry.sha256))).length === 0,
    'SDP-carrying download deleted'
  );
  // Board library listing.
  ok(
    missingTargetLibraries(qnxEntry, '/usr/lib:\nlibc.so.6\nlibc++.so.2\nlibEGL.so.1\n').join() ===
      'libscreen.so.1',
    'missing board library reported'
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
  const boardOk = path.join(temporary, 'board-ok.txt');
  await writeFile(boardOk, '/usr/lib:\nlibc.so.6\nlibc++.so.2\nlibscreen.so.1\nlibEGL.so.1\n');
  const boardShort = path.join(temporary, 'board-short.txt');
  await writeFile(boardShort, 'libc.so.6\n');
  const qnxArgs = [
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
    '--sdp',
    '8.0.5',
  ];
  const unchecked = JSON.parse(await quiet(() => runnerMain(qnxArgs)));
  ok(
    unchecked.targetLibrariesChecked === false && unchecked.targetLibraries.length === 4,
    'QNX fetch reports target libraries unchecked'
  );
  const checked = JSON.parse(await quiet(() => runnerMain([...qnxArgs, '--target-libs', boardOk])));
  ok(checked.targetLibrariesChecked === true, 'QNX fetch checks board listing');
  await rejects(
    quiet(() => runnerMain([...qnxArgs, '--target-libs', boardShort])),
    /lacks libraries.*libc\+\+\.so\.2.*licensed QNX SDP target image/
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
