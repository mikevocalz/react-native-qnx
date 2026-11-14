import assert from 'node:assert/strict';
import { mkdtempSync, readFileSync, rmSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { tmpdir } from 'node:os';
import path from 'node:path';

const directory = mkdtempSync(path.join(tmpdir(), 'rnqnx-rnsvg-corpus-'));
try {
  const layoutPath = path.join(directory, 'layout.json');
  const generated = spawnSync(
    process.execPath,
    ['scripts/rnsvg-reference.mjs', '--corpus', directory, '--layout', layoutPath],
    { encoding: 'utf8' }
  );
  assert.equal(generated.status, 0, generated.stderr);
  const manifest = JSON.parse(readFileSync('tests/fixtures/rnsvg/manifest.json', 'utf8'));
  const layout = JSON.parse(readFileSync(layoutPath, 'utf8'));
  const expected = manifest.fixtures.filter(
    /** @param {any} fixture */ (fixture) => fixture.reference !== 'none'
  );
  const names = layout.batches.flat().map(/** @param {any} cell */ (cell) => cell.name);
  assert.deepEqual(
    names,
    expected.map(/** @param {any} fixture */ (fixture) => fixture.name),
    'Packing preserves every reference fixture once, in manifest order'
  );
  for (const batch of layout.batches) {
    assert.ok(batch.length <= 12);
    const counts = batch.map(
      /** @param {any} cell */ (cell) => {
        const fixture = expected.find(
          /** @param {any} fixture */ (fixture) => fixture.name === cell.name
        );
        const xml = readFileSync(path.join('tests/fixtures/rnsvg', fixture.svg), 'utf8');
        return (xml.match(/<[A-Za-z_]/g) || []).length;
      }
    );
    assert.ok(
      batch.length === 1 ||
        counts.reduce(/** @param {number} a @param {number} b */ (a, b) => a + b, 0) <= 1000,
      'Large Fabric scenes must mount in their own batch'
    );
  }
  const shards = Array.from({ length: 4 }, () => /** @type {string[]} */ ([]));
  layout.batches.forEach(
    /** @param {any[]} batch @param {number} index */ (batch, index) => {
      shards[index % 4].push(...batch.map((cell) => cell.name));
    }
  );
  assert.equal(new Set(shards.flat()).size, expected.length);
  assert.equal(shards.flat().length, expected.length);
  assert.ok(shards.every((shard) => shard.length > 0));
  console.log(
    `SVG corpus packing: ${expected.length} fixtures, ${layout.batches.length} bounded batches, four nonempty shards`
  );
} finally {
  rmSync(directory, { recursive: true, force: true });
}
