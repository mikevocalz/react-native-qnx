#!/usr/bin/env node
// Runs the project's installed Metro with the documented runBuild({assets: true})
// API. Separate process so Metro workers stay in one supervised process group.
import { writeFile } from 'node:fs/promises';
import { createRequire } from 'node:module';

const [request] = process.argv.slice(2);
const { manifest, entry, config, platform, out, assetsOut } = JSON.parse(request ?? '{}');
for (const value of [manifest, entry, config, platform, out, assetsOut]) {
  if (typeof value !== 'string' || !value) throw new Error('Invalid Metro asset worker request.');
}
const require = createRequire(manifest);
const Metro = require('metro');
const loaded = await Metro.loadConfig({ config, maxWorkers: 2 });
const result = await Metro.runBuild(loaded, {
  entry,
  platform,
  dev: false,
  minify: true,
  out,
  assets: true,
});
const assets = (result.assets ?? []).map((/** @type {any} */ asset) => ({
  httpServerLocation: asset.httpServerLocation,
  name: asset.name,
  type: asset.type,
  scales: asset.scales,
  files: asset.files,
}));
await writeFile(assetsOut, JSON.stringify(assets), { flag: 'wx', mode: 0o600 });
