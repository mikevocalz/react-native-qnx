// The Metro hook that replaces react-native-svg source files on QNX: which
// files it maps, that it leaves every other platform and file alone, and that
// an overlay file can still reach the original it replaces.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { createRequire } from 'node:module';
import path from 'node:path';

const root = process.cwd();
const require = createRequire(import.meta.url);
const { createSvgOverlayResolveRequest, scanOverlay, OVERLAY_ROOT } = require(
  path.join(root, 'packages/react-native-qnx/svg/resolver.js')
);
const { getQnxMetroConfig } = require(path.join(root, 'packages/metro-config-qnx/src/index.js'));
const svgRoot = path.dirname(require.resolve('react-native-svg/package.json'));

// Every file the overlay replaces. Adding or dropping one is a decision for
// this list, because the overlay and the Fabric specs in codegen move together.
const expected = [
  'elements/filters/Filter',
  ...[
    'FeComponentTransfer',
    'FeConvolveMatrix',
    'FeDiffuseLighting',
    'FeDisplacementMap',
    'FeDropShadow',
    'FeImage',
    'FeMorphology',
    'FeSpecularLighting',
    'FeTile',
    'FeTurbulence',
  ].map((name) => `elements/filters/${name}`),
  ...[
    'FeBlend',
    'FeColorMatrix',
    'FeComposite',
    'FeFlood',
    'FeGaussianBlur',
    'FeMerge',
    'FeOffset',
    'LinearGradient',
    'RadialGradient',
  ].map((name) => `fabric/${name}NativeComponent`),
  'lib/extract/extractFilter',
  'lib/extract/extractGradient',
  'lib/extract/extractText',
  'xmlTags',
]
  .filter((key, index, all) => all.indexOf(key) === index)
  .sort();

const overlay = scanOverlay(OVERLAY_ROOT);
assert.deepEqual([...overlay.keys()].sort(), expected);

/**
 * The package's own file for an overlay key: the one the overlay replaces.
 * @param {string} key
 */
function upstreamFile(key) {
  for (const extension of ['.tsx', '.ts']) {
    const file = path.join(svgRoot, 'src', `${key}${extension}`);
    if (fs.existsSync(file)) return file;
  }
  throw new Error(`overlay file has no counterpart in react-native-svg: ${key}`);
}

/** @param {string} filePath */
const returning = (filePath) => () => ({ type: 'sourceFile', filePath });
const context = { originModulePath: path.join(svgRoot, 'src/elements/Rect.tsx') };

for (const [key, replacement] of overlay) {
  const original = upstreamFile(key);
  const resolve = createSvgOverlayResolveRequest(returning(original));
  assert.deepEqual(
    resolve(context, './whatever', 'qnx'),
    { type: 'sourceFile', filePath: replacement },
    `${key} maps to the overlay on qnx`
  );
  for (const platform of ['ios', 'android', 'web', 'marathon', null]) {
    assert.deepEqual(
      resolve(context, './whatever', platform),
      { type: 'sourceFile', filePath: original },
      `${key} is untouched on ${platform}`
    );
  }
}

// The platform list is configurable and has no default beyond qnx.
{
  const original = upstreamFile('elements/filters/FeTile');
  const resolve = createSvgOverlayResolveRequest(returning(original), {
    platforms: ['marathon'],
  });
  assert.equal(
    resolve(context, './x', 'marathon').filePath,
    overlay.get('elements/filters/FeTile')
  );
  assert.equal(resolve(context, './x', 'qnx').filePath, original);
}

// Only files under react-native-svg/src are mapped.
for (const filePath of [
  path.join(svgRoot, 'src/elements/Rect.tsx'),
  path.join(svgRoot, 'src/elements/filters/FeFlood.tsx'),
  path.join(svgRoot, 'lib/commonjs/xmlTags.js'),
  '/app/node_modules/other-package/src/xmlTags.ts',
  '/app/node_modules/react-native-svg-extras/src/xmlTags.ts',
]) {
  const resolve = createSvgOverlayResolveRequest(returning(filePath));
  assert.equal(resolve(context, './x', 'qnx').filePath, filePath, filePath);
}
{
  const notSource = { type: 'empty' };
  assert.equal(createSvgOverlayResolveRequest(() => notSource)(context, 'x', 'qnx'), notSource);
  const asset = { type: 'assetFiles', filePaths: ['/a.png'] };
  assert.equal(createSvgOverlayResolveRequest(() => asset)(context, 'x', 'qnx'), asset);
}

// pnpm and Windows path shapes.
{
  const pnpm =
    '/app/node_modules/.pnpm/react-native-svg@15.16.0_react-native@0.88.1/node_modules/react-native-svg/src/xmlTags.ts';
  const windows = 'C:\\app\\node_modules\\react-native-svg\\src\\lib\\extract\\extractGradient.ts';
  assert.equal(
    createSvgOverlayResolveRequest(returning(pnpm))(context, './xmlTags', 'qnx').filePath,
    overlay.get('xmlTags')
  );
  assert.equal(
    createSvgOverlayResolveRequest(returning(windows))(context, './x', 'qnx').filePath,
    overlay.get('lib/extract/extractGradient')
  );
}

// An overlay file reaches the original it replaces, and every package file, by
// the package's public name. The package may not be installed beside the
// overlay, so the request is resolved from the package root Metro already saw.
{
  /** @type {string[]} */
  const asked = [];
  const original = upstreamFile('lib/extract/extractGradient');
  const resolve = createSvgOverlayResolveRequest(
    (/** @type {unknown} */ _context, /** @type {string} */ moduleName) => {
      asked.push(moduleName);
      return {
        type: 'sourceFile',
        filePath: moduleName.startsWith('/') ? moduleName + '.ts' : original,
      };
    }
  );
  const fromOverlay = { originModulePath: overlay.get('lib/extract/extractGradient') };

  // Nothing learned yet: the request goes through unchanged.
  resolve(fromOverlay, 'react-native-svg/src/lib/extract/extractGradient', 'qnx');
  assert.equal(asked.at(-1), 'react-native-svg/src/lib/extract/extractGradient');

  // The package's own import is redirected to the overlay, which teaches the root.
  assert.equal(
    resolve(context, '../lib/extract/extractGradient', 'qnx').filePath,
    overlay.get('lib/extract/extractGradient')
  );

  const own = resolve(fromOverlay, 'react-native-svg/src/lib/extract/extractGradient', 'qnx');
  assert.equal(asked.at(-1), path.join(svgRoot, 'src/lib/extract/extractGradient'));
  assert.equal(own.filePath, path.join(svgRoot, 'src/lib/extract/extractGradient') + '.ts');
  assert.notEqual(own.filePath, overlay.get('lib/extract/extractGradient'), 'no recursion');

  // Other modules an overlay file imports go to the fallback as written.
  resolve(fromOverlay, 'react', 'qnx');
  assert.equal(asked.at(-1), 'react');
  // On another platform the public-name rewrite does not run.
  resolve(fromOverlay, 'react-native-svg/src/xmlTags', 'ios');
  assert.equal(asked.at(-1), 'react-native-svg/src/xmlTags');
}

// getQnxMetroConfig installs the hook ahead of the package resolver, for qnx only.
{
  const original = upstreamFile('elements/filters/FeMorphology');
  const config = getQnxMetroConfig(root);
  const metroContext = {
    originModulePath: path.join(svgRoot, 'src/elements.ts'),
    resolveRequest: () => ({ type: 'sourceFile', filePath: original }),
  };
  assert.equal(
    config.resolver.resolveRequest(metroContext, './elements/filters/FeMorphology', 'qnx').filePath,
    overlay.get('elements/filters/FeMorphology')
  );
  assert.equal(
    config.resolver.resolveRequest(metroContext, './elements/filters/FeMorphology', 'ios').filePath,
    original
  );
}

console.log(`react-native-svg overlay resolver: ${overlay.size} files, qnx only`);
