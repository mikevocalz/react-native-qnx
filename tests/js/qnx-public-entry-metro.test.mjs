import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createRequire } from 'node:module';
import { mkdtemp, readFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';

const require = createRequire(import.meta.url);
const root = process.cwd();
const directory = await mkdtemp(path.join(tmpdir(), 'rnqnx-public-metro-'));
const manifestPath = require.resolve('metro/package.json');
const manifest = JSON.parse(await readFile(manifestPath, 'utf8'));
const bin = typeof manifest.bin === 'string' ? manifest.bin : manifest.bin.metro;
try {
  for (const platform of ['qnx', 'marathon']) {
    const config =
      platform === 'qnx' ? 'metro.public.config.js' : 'metro.public.marathon.config.js';
    const output = path.join(directory, `${platform}.js`);
    const result = spawnSync(
      process.execPath,
      [
        path.resolve(path.dirname(manifestPath), bin),
        'build',
        'public-api-probe.js',
        '--config',
        config,
        '--platform',
        platform,
        '--dev',
        'false',
        '--max-workers',
        '2',
        '--out',
        output,
      ],
      {
        cwd: path.join(root, 'apps/rn-playground'),
        encoding: 'utf8',
        timeout: 90000,
        maxBuffer: 4 * 1024 * 1024,
      }
    );
    assert.equal(result.error, undefined);
    assert.equal(result.status, 0, result.stdout + result.stderr);
    const bundled = await readFile(output, 'utf8');
    assert.ok(bundled.includes('RNQNX_PUBLIC_API_READY'));
    assert.ok(bundled.length > 10000);
    console.log(
      `public-entry ${platform}: ${Buffer.byteLength(bundled)} bytes (real Metro, runtime separate)`
    );
  }
} finally {
  await rm(directory, { recursive: true, force: true });
}
