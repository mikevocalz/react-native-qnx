import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const fromExpo = createRequire(require.resolve('expo/package.json'));
const fromMetro = createRequire(fromExpo.resolve('@expo/metro-config/package.json'));
const fromPostcss = createRequire(fromMetro.resolve('postcss/package.json'));
const { SourceMapConsumer, SourceMapGenerator } = fromPostcss('source-map-js');
const basic = { version: 3, sources: ['input.js'], names: [], mappings: 'AAAA' };
/** @param {unknown} line @param {unknown} [column] @param {object} [map] */
const indexed = (line, column = 0, map = basic) => ({
  version: 3,
  sections: [{ offset: { line, column }, map }],
});

// GHSA-68fv-2mgg-jv7q: reject oversized and invalid section offsets before
// serializing a map. Testing construction avoids blocking on vulnerable code.
for (const line of [10000001, Infinity, NaN, -1, 0.5, '1']) {
  assert.throws(() => new SourceMapConsumer(indexed(line)), /offset/i);
}
for (const column of [Infinity, NaN, -1, 0.5, '1']) {
  assert.throws(() => new SourceMapConsumer(indexed(0, column)), /offset/i);
}
assert.throws(() => new SourceMapConsumer(indexed(6000000, 0, indexed(6000000))), /offset/i);
const consumer = new SourceMapConsumer(indexed(2));
/** @type {unknown[]} */
const mappings = [];
consumer.eachMapping((/** @type {unknown} */ mapping) => mappings.push(mapping));
assert.deepEqual(mappings, [
  {
    source: 'input.js',
    generatedLine: 3,
    generatedColumn: 0,
    originalLine: 1,
    originalColumn: 0,
    name: null,
  },
]);
const flat = new SourceMapConsumer(basic);
assert.equal(SourceMapGenerator.fromSourceMap(flat).toJSON().mappings, 'AAAA');
assert.deepEqual(flat.originalPositionFor({ line: 1, column: 0 }), {
  source: 'input.js',
  line: 1,
  column: 0,
  name: null,
});
