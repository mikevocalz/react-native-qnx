#!/usr/bin/env node
// Regenerates the resvg reference images for the react-native-svg fixture
// corpus, or checks that they are current.
//
//   node scripts/rnsvg-reference.mjs            render every golden, update sha256 and size in the manifest
//   node scripts/rnsvg-reference.mjs --check    fail if a golden is missing, a source hash differs from
//                                               the manifest (the golden was rendered from another
//                                               version of the source), or the resvg version differs
//                                               from the manifest
//   node scripts/rnsvg-reference.mjs --check --mtime
//                                               also fail if a golden file is older than its source;
//                                               only meaningful in a working tree, since a fresh git
//                                               checkout gives every file a new modification time
//   node scripts/rnsvg-reference.mjs Circle-1   render only the named fixtures
//   node scripts/rnsvg-reference.mjs --corpus <dir> --layout <file>
//                                               write the generated JS module (<dir>/rnsvg-corpus-data.js)
//                                               and the cell layout (<file>) for the real-path runner
//                                               (apps/rn-playground/rnsvg-corpus.js and
//                                               tests/native/qt-rnsvg-fabric/RnsvgCorpusFabricTest.cpp)
//
// The manifest is tests/fixtures/rnsvg/manifest.json. Goldens are written to
// tests/fixtures/rnsvg/golden/<name>.png at 1x on a white background, using
// the DejaVu fonts in the manifest's fontDirectory and no system fonts, so the
// output does not depend on the machine. Goldens are never edited by hand.
import { execFileSync, spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const fixtureDir = path.join(repo, 'tests/fixtures/rnsvg');
const manifestPath = path.join(fixtureDir, 'manifest.json');
const RESVG = process.env.RESVG || '/opt/homebrew/bin/resvg';

const args = process.argv.slice(2);
const check = args.includes('--check');
const checkMtime = args.includes('--mtime');
const valueOf = (flag) => (args.includes(flag) ? args[args.indexOf(flag) + 1] : undefined);
const corpusDir = valueOf('--corpus');
const layoutFile = valueOf('--layout');
const only = args.filter(
  (a, i) => !a.startsWith('--') && !(i > 0 && ['--corpus', '--layout'].includes(args[i - 1]))
);

// Arrays of scalars stay on one line so the manifest diffs readably.
function format(value, indent = 0) {
  const pad = '  '.repeat(indent);
  if (Array.isArray(value)) {
    if (value.every((v) => v === null || typeof v !== 'object')) return JSON.stringify(value);
    return (
      '[\n' + value.map((v) => pad + '  ' + format(v, indent + 1)).join(',\n') + '\n' + pad + ']'
    );
  }
  if (value && typeof value === 'object') {
    const keys = Object.keys(value);
    if (keys.length === 0) return '{}';
    return (
      '{\n' +
      keys.map((k) => `${pad}  ${JSON.stringify(k)}: ${format(value[k], indent + 1)}`).join(',\n') +
      '\n' +
      pad +
      '}'
    );
  }
  return JSON.stringify(value);
}

function resvgVersion() {
  const r = spawnSync(RESVG, ['--version'], { encoding: 'utf8' });
  if (r.status !== 0) throw new Error(`cannot run ${RESVG}: ${r.error || r.stderr}`);
  return r.stdout.trim().split(/\s+/).pop();
}

function pngSize(file) {
  const b = fs.readFileSync(file);
  return [b.readUInt32BE(16), b.readUInt32BE(20)];
}

const sha256 = (file) => createHash('sha256').update(fs.readFileSync(file)).digest('hex');

const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));

// Cells of at most 12 fixtures per batch, packed in rows into a window of
// WINDOW_W x WINDOW_H, in manifest order. Fixtures without a reference image
// are not drawn: there is nothing to compare them with.
const WINDOW_W = 1100,
  WINDOW_H = 1000,
  MARGIN = 6,
  BATCH_SIZE = 12;
function packBatches(fixtures) {
  const batches = [];
  let cells = [],
    x = MARGIN,
    y = MARGIN,
    rowH = 0;
  const flush = () => {
    if (cells.length) batches.push(cells);
    cells = [];
    x = MARGIN;
    y = MARGIN;
    rowH = 0;
  };
  for (const fx of fixtures) {
    if (!fx.size) throw new Error(`${fx.name}: no size in the manifest; render the goldens first`);
    const [w, h] = fx.size;
    if (w + 2 * MARGIN > WINDOW_W || h + 2 * MARGIN > WINDOW_H)
      throw new Error(`${fx.name}: ${w}x${h} does not fit the ${WINDOW_W}x${WINDOW_H} window`);
    if (x + w + MARGIN > WINDOW_W) {
      x = MARGIN;
      y += rowH + MARGIN;
      rowH = 0;
    }
    if (cells.length >= BATCH_SIZE || y + h + MARGIN > WINDOW_H) {
      flush();
    }
    cells.push({ name: fx.name, x, y, w, h });
    x += w + MARGIN;
    rowH = Math.max(rowH, h);
  }
  flush();
  return batches;
}

if (corpusDir) {
  if (!layoutFile) throw new Error('--corpus needs --layout');
  const drawn = manifest.fixtures.filter((f) => f.reference !== 'none');
  const batches = packBatches(drawn);
  const byName = new Map(drawn.map((f) => [f.name, f]));
  fs.mkdirSync(corpusDir, { recursive: true });
  const data = batches.map((cells) =>
    cells.map((c) => ({
      ...c,
      css: !!byName.get(c.name).css,
      xml: fs.readFileSync(path.join(fixtureDir, byName.get(c.name).svg), 'utf8'),
    }))
  );
  fs.writeFileSync(
    path.join(corpusDir, 'rnsvg-corpus-data.js'),
    '// Generated by scripts/rnsvg-reference.mjs --corpus. Do not edit.\nmodule.exports = ' +
      JSON.stringify({ batches: data }) +
      ';\n'
  );
  fs.mkdirSync(path.dirname(layoutFile), { recursive: true });
  fs.writeFileSync(
    layoutFile,
    JSON.stringify({ window: [WINDOW_W, WINDOW_H], batches }, null, 1) + '\n'
  );
  console.log(`rnsvg-reference: ${drawn.length} fixtures in ${batches.length} batches`);
  process.exit(0);
}

const version = resvgVersion();
const problems = [];

if (version !== manifest.reference.version) {
  problems.push(`resvg is ${version}, the manifest records ${manifest.reference.version}`);
}

const fontDir = path.join(repo, manifest.reference.fontDirectory);
let rendered = 0;
for (const fx of manifest.fixtures) {
  if (only.length && !only.includes(fx.name)) continue;
  const src = path.join(fixtureDir, fx.svg);
  if (!fs.existsSync(src)) {
    problems.push(`${fx.name}: missing source ${fx.svg}`);
    continue;
  }
  const hash = sha256(src);
  if (fx.reference === 'none') {
    if (check && fx.sha256 !== hash)
      problems.push(`${fx.name}: source changed since the manifest was written`);
    fx.sha256 = hash;
    continue;
  }
  const golden = path.join(fixtureDir, fx.golden);
  if (check) {
    if (fx.sha256 !== hash) problems.push(`${fx.name}: source hash differs from the manifest`);
    if (!fs.existsSync(golden)) problems.push(`${fx.name}: golden is missing`);
    else if (checkMtime && fs.statSync(golden).mtimeMs + 1000 < fs.statSync(src).mtimeMs)
      problems.push(`${fx.name}: golden is older than its source`);
    continue;
  }
  fs.mkdirSync(path.dirname(golden), { recursive: true });
  execFileSync(
    RESVG,
    [
      '--skip-system-fonts',
      '--use-fonts-dir',
      fontDir,
      '--font-family',
      manifest.reference.defaultFontFamily,
      '--background',
      manifest.reference.background,
      src,
      golden,
    ],
    { stdio: ['ignore', 'ignore', 'ignore'] }
  );
  const [w, h] = pngSize(golden);
  fx.sha256 = hash;
  fx.size = [w, h];
  rendered++;
}

if (check) {
  if (problems.length) {
    console.error(problems.join('\n'));
    console.error(
      `rnsvg-reference: ${problems.length} problem(s). Run node scripts/rnsvg-reference.mjs to regenerate.`
    );
    process.exit(1);
  }
  console.log(`rnsvg-reference: ${manifest.fixtures.length} fixtures current (resvg ${version})`);
} else {
  if (problems.length) {
    console.error(problems.join('\n'));
    process.exit(1);
  }
  fs.writeFileSync(manifestPath, format(manifest) + '\n');
  console.log(`rnsvg-reference: rendered ${rendered} goldens with resvg ${version}`);
}
