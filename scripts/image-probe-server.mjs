#!/usr/bin/env node
// Serves one JPEG for apps/performance-lab/image-probe.js and logs, per request,
// when it began and finished sending on its own monotonic clock. The fetch share
// of a load is read from this log, not guessed from the client.
//   node scripts/image-probe-server.mjs --file <jpeg> [--port 8099] [--delay-ms 0] [--host 127.0.0.1]

import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { performance } from 'node:perf_hooks';

const args = new Map();
for (let i = 2; i < process.argv.length; i += 2) args.set(process.argv[i], process.argv[i + 1]);
const file = args.get('--file');
if (!file) {
  console.error(
    'usage: image-probe-server.mjs --file <jpeg> [--port 8099] [--delay-ms 0] [--host 127.0.0.1]'
  );
  process.exit(2);
}
const port = Number(args.get('--port') ?? 8099);
const delayMs = Number(args.get('--delay-ms') ?? 0);
const host = args.get('--host') ?? '127.0.0.1';
const body = await readFile(file);

createServer((req, res) => {
  const begin = performance.now();
  const send = () => {
    res.writeHead(200, {
      'content-type': 'image/jpeg',
      'content-length': body.length,
      'cache-control': 'public, max-age=3600',
    });
    res.end(body, () => {
      const end = performance.now();
      console.log(
        JSON.stringify({
          url: req.url,
          beginMs: +begin.toFixed(3),
          sendMs: +(end - begin).toFixed(3),
          bytes: body.length,
        })
      );
    });
  };
  if (delayMs > 0) setTimeout(send, delayMs);
  else send();
}).listen(port, host, () =>
  console.log(JSON.stringify({ listening: `${host}:${port}`, bytes: body.length, delayMs }))
);
