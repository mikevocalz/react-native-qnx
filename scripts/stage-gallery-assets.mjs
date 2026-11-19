#!/usr/bin/env node
// Stage the bundled assets a QNX app's JavaScript require()s, at the paths the
// host resolves them from.
//
// `bundle-qnx` emits JavaScript only. Metro gives each asset a
// httpServerLocation of `/assets/<directory relative to the app root>`, and
// the host opens that path relative to the executable, so assets/icon.png in
// an app is read from <deploy dir>/assets/assets/icon.png.
//
// Usage:
//   node scripts/stage-gallery-assets.mjs --out <deploy dir> [--app <app dir>]
//   node scripts/stage-gallery-assets.mjs --check <deploy dir> [--app <app dir>]
//
// Staging copies every statically required asset. --check exits 1 when one is
// missing from the deploy dir or differs from its source. A require() of an
// asset that does not exist in the app fails both modes.
import { createHash } from 'node:crypto';
import { cp, mkdir, readdir, readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// Metro's default assetExts.
const ASSET_EXTENSIONS = new Set(
  (
    'bmp gif jpg jpeg png psd svg webp xml m4v mov mp4 mpeg mpg webm aac aiff caf m4a mp3 wav ' +
    'html pdf yaml yml otf ttf zip'
  )
    .split(' ')
    .map((ext) => '.' + ext)
);
const SOURCE_EXTENSIONS = new Set(['.js', '.jsx', '.mjs', '.cjs', '.ts', '.tsx']);
const SKIPPED_DIRECTORIES = new Set(['node_modules', 'build', '.git']);
const REQUIRE = /\brequire\(\s*(['"])(\.{1,2}\/[^'"]+)\1\s*\)/g;

async function* sourceFiles(directory) {
  for (const entry of await readdir(directory, { withFileTypes: true })) {
    if (entry.isDirectory()) {
      if (!SKIPPED_DIRECTORIES.has(entry.name))
        yield* sourceFiles(path.join(directory, entry.name));
    } else if (SOURCE_EXTENSIONS.has(path.extname(entry.name))) {
      yield path.join(directory, entry.name);
    }
  }
}

const sha256 = async (file) =>
  createHash('sha256')
    .update(await readFile(file))
    .digest('hex');

/**
 * Every asset the app's sources require(), as { source, staged, referencedBy }.
 * `staged` is relative to the deploy directory. Throws on a missing asset.
 * @param {string} appRoot
 */
export async function referencedAssets(appRoot) {
  const root = path.resolve(appRoot);
  /** @type {Map<string, { source: string, staged: string, referencedBy: string[] }>} */
  const found = new Map();
  const missing = [];
  for await (const file of sourceFiles(root)) {
    const text = await readFile(file, 'utf8');
    for (const match of text.matchAll(REQUIRE)) {
      const specifier = match[2];
      if (!ASSET_EXTENSIONS.has(path.extname(specifier).toLowerCase())) continue;
      const source = path.resolve(path.dirname(file), specifier);
      const where = path.relative(root, file);
      const relative = path.relative(root, source);
      if (relative.startsWith('..') || path.isAbsolute(relative)) {
        throw new Error(`${where} requires ${specifier}, which is outside the app root ${root}`);
      }
      const staged = path.posix.join(
        'assets',
        ...path
          .dirname(relative)
          .split(path.sep)
          .filter((part) => part !== '.'),
        path.basename(relative)
      );
      const known = found.get(source) ?? { source, staged, referencedBy: [] };
      known.referencedBy.push(where);
      found.set(source, known);
      try {
        await readFile(source);
      } catch {
        missing.push(`${where} requires ${specifier}, but ${relative} does not exist`);
      }
    }
  }
  if (missing.length) throw new Error([...new Set(missing)].join('\n'));
  return [...found.values()].sort((a, b) => a.staged.localeCompare(b.staged));
}

/**
 * Assets that are not in `deployDir` with the source bytes.
 * @param {string} appRoot
 * @param {string} deployDir
 */
export async function missingFromDeploy(appRoot, deployDir) {
  const problems = [];
  for (const asset of await referencedAssets(appRoot)) {
    const target = path.join(deployDir, asset.staged);
    let actual;
    try {
      actual = await sha256(target);
    } catch {
      problems.push(`not staged: ${asset.staged} (required by ${asset.referencedBy.join(', ')})`);
      continue;
    }
    if (actual !== (await sha256(asset.source))) {
      problems.push(
        `stale: ${asset.staged} differs from ${path.relative(process.cwd(), asset.source)}`
      );
    }
  }
  return problems;
}

/**
 * @param {string} appRoot
 * @param {string} deployDir
 */
export async function stageAssets(appRoot, deployDir) {
  const assets = await referencedAssets(appRoot);
  for (const asset of assets) {
    const target = path.join(deployDir, asset.staged);
    await mkdir(path.dirname(target), { recursive: true });
    await cp(asset.source, target);
  }
  return assets.map((asset) => asset.staged);
}

async function main(argv) {
  const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
  let app = path.join(repoRoot, 'apps/component-gallery');
  let out = null;
  let check = null;
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === '--app') app = path.resolve(argv[++i] ?? '');
    else if (argv[i] === '--out') out = path.resolve(argv[++i] ?? '');
    else if (argv[i] === '--check') check = path.resolve(argv[++i] ?? '');
    else throw new Error(`unknown argument: ${argv[i]}`);
  }
  if (!out === !check) throw new Error('pass exactly one of --out <dir> or --check <dir>');
  if (check) {
    const problems = await missingFromDeploy(app, check);
    if (problems.length) {
      console.error(problems.join('\n'));
      process.exitCode = 1;
      return;
    }
    console.log(`all referenced assets are staged in ${check}`);
    return;
  }
  for (const staged of await stageAssets(app, out)) console.log(`staged ${staged}`);
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  main(process.argv.slice(2)).catch((error) => {
    console.error(error.message);
    process.exitCode = 1;
  });
}
