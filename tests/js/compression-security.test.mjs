import assert from 'node:assert/strict';
import { once } from 'node:events';
import http from 'node:http';
import { createRequire } from 'node:module';
import { setImmediate } from 'node:timers/promises';
import zlib from 'node:zlib';

const require = createRequire(import.meta.url);
const fromExpo = createRequire(require.resolve('expo/package.json'));
const fromCli = createRequire(fromExpo.resolve('@expo/cli/package.json'));
const compression = fromCli('compression');
const body = 'QNX development response\n'.repeat(1024);

/** @param {string} url @param {string} encoding @returns {Promise<http.IncomingMessage>} */
function get(url, encoding) {
  return new Promise((resolve, reject) => {
    const request = http.get(url, { headers: { 'Accept-Encoding': encoding } }, resolve);
    request.on('error', reject);
    request.setTimeout(5000, () => request.destroy(new Error('HTTP fixture timed out')));
  });
}

// Observe the public zlib stream lifecycle while exercising real HTTP and
// compression middleware. No heap-size thresholds or garbage-collector timing.
/** @type {Array<[string, string, (input: Buffer) => Buffer]>} */
const cases = [
  ['gzip', 'createGzip', zlib.gunzipSync],
  ['deflate', 'createDeflate', zlib.inflateSync],
  ['br', 'createBrotliCompress', zlib.brotliDecompressSync],
];
for (const [encoding, factory, decode] of cases) {
  const descriptor = Object.getOwnPropertyDescriptor(zlib, factory);
  assert.ok(descriptor);
  /** @type {import('node:stream').Transform[]} */
  const streams = [];
  Object.defineProperty(zlib, factory, {
    ...descriptor,
    /** @param {unknown[]} args */
    value(...args) {
      const stream = descriptor.value(...args);
      streams.push(stream);
      return stream;
    },
  });
  const middleware = compression({ threshold: 0 });
  let abortedResponseClosed = () => {};
  /** @type {Promise<void>} */
  const aborted = new Promise((resolve) => {
    abortedResponseClosed = resolve;
  });
  const server = http.createServer((req, res) => {
    res.setHeader('Content-Type', 'text/plain');
    middleware(req, res, () => {
      if (req.url === '/abort') {
        res.once('close', abortedResponseClosed);
        res.write(body);
        assert.ok('flush' in res && typeof res.flush === 'function');
        res.flush();
      } else {
        res.end(body);
      }
    });
  });
  try {
    server.listen(0, '127.0.0.1');
    await once(server, 'listening');
    const address = server.address();
    assert.ok(address && typeof address !== 'string');
    const url = `http://127.0.0.1:${address.port}`;
    const response = await get(`${url}/complete`, encoding);
    const chunks = [];
    for await (const chunk of response) chunks.push(chunk);
    assert.equal(response.headers['content-encoding'], encoding);
    assert.equal(decode(Buffer.concat(chunks)).toString(), body);

    const interrupted = await get(`${url}/abort`, encoding);
    interrupted.destroy();
    await Promise.race([
      aborted,
      new Promise((_, reject) => {
        const timer = setTimeout(() => reject(new Error('Response did not close')), 5000);
        timer.unref();
        aborted.then(() => clearTimeout(timer));
      }),
    ]);
    await setImmediate();
    assert.equal(streams.length, 2, `${encoding}: real compression was exercised`);
    assert.equal(streams[1].destroyed, true, `${encoding}: aborted stream must be destroyed`);
  } finally {
    for (const stream of streams) stream.destroy();
    server.closeAllConnections();
    await new Promise((resolve) => server.close(resolve));
    Object.defineProperty(zlib, factory, descriptor);
  }
}
