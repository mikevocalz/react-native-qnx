#!/usr/bin/env node
// Generates the Fabric C++ props/events for the react-native-svg components
// this host mounts, from the pinned package's own TypeScript specs. Same
// determinism contract as qt-svg-codegen.mjs: --write refreshes, --check fails
// on drift. Shadow nodes and descriptors come from react-native-svg's own
// common/cpp sources, so only props, events and the registration list are
// generated here.
import { createRequire } from 'node:module';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const require = createRequire(path.join(root, 'package.json'));
const rnRequire = createRequire(require.resolve('react-native/package.json'));
const codegenRoot = path.dirname(rnRequire.resolve('@react-native/codegen/package.json'));
const version = JSON.parse(fs.readFileSync(path.join(codegenRoot, 'package.json'), 'utf8')).version;
if (version !== '0.88.1')
  throw new Error(`Re-audit react-native-svg generation for codegen ${version}`);
const svgRoot = path.dirname(require.resolve('react-native-svg/package.json'));
const svgVersion = JSON.parse(fs.readFileSync(path.join(svgRoot, 'package.json'), 'utf8')).version;
if (svgVersion !== '15.16.0')
  throw new Error(`Re-audit react-native-svg generation for ${svgVersion}`);
const { TypeScriptParser } = require(path.join(codegenRoot, 'lib/parsers/typescript/parser.js'));
const { generate } = require(path.join(codegenRoot, 'lib/generators/RNCodegen.js'));
const mode = process.argv[2];
if (!['--write', '--check'].includes(mode) || process.argv.length !== 3) {
  throw new Error('Usage: node scripts/qt-rnsvg-codegen.mjs --write|--check');
}
// Props for every component the package declares. The package's own
// RNSVGShadowNodes.h refers to all of them, so all must be generated, but the
// host registers (and so mounts) only the supported subset listed in
// vnext/src/fabric/QtRnsvgBridge.cpp. The Android-only view is skipped.
const specs = [
  'IOSSvgViewNativeComponent',
  'CircleNativeComponent',
  'ClipPathNativeComponent',
  'DefsNativeComponent',
  'EllipseNativeComponent',
  'FeBlendNativeComponent',
  'FeColorMatrixNativeComponent',
  'FeCompositeNativeComponent',
  'FeFloodNativeComponent',
  'FeGaussianBlurNativeComponent',
  'FeMergeNativeComponent',
  'FeOffsetNativeComponent',
  'FilterNativeComponent',
  'ForeignObjectNativeComponent',
  'GroupNativeComponent',
  'ImageNativeComponent',
  'LinearGradientNativeComponent',
  'LineNativeComponent',
  'MarkerNativeComponent',
  'MaskNativeComponent',
  'PathNativeComponent',
  'PatternNativeComponent',
  'RadialGradientNativeComponent',
  'RectNativeComponent',
  'SymbolNativeComponent',
  'TextNativeComponent',
  'TextPathNativeComponent',
  'TSpanNativeComponent',
  'UseNativeComponent',
];
const output = path.join(root, 'vnext/generated/react/renderer/components/rnsvg');
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'rnqnx-rnsvg-'));
try {
  const parser = new TypeScriptParser();
  const modules = {};
  for (const name of specs) {
    let file = path.join(svgRoot, 'src/fabric', `${name}.ts`);
    if (/GradientNativeComponent$/.test(name)) {
      // The gradient stop colors are packed ARGB integers carried in this
      // array. React Native's Float is 32-bit on this host, which cannot hold
      // a packed color that has any alpha below 255 (blue would be rounded
      // away), so the stops are parsed as doubles instead.
      const source = fs.readFileSync(file, 'utf8');
      const patched = source.replace(
        'gradient?: ReadonlyArray<CodegenTypes.Float>;',
        'gradient?: ReadonlyArray<CodegenTypes.Double>;'
      );
      if (patched === source) throw new Error(`gradient prop not found in ${name}`);
      file = path.join(temporary, `${name}.ts`);
      fs.writeFileSync(file, patched);
    }
    const schema = parser.parseFile(file);
    Object.assign(modules, schema.modules);
  }
  if (
    !generate(
      { libraryName: 'rnsvg', schema: { modules }, outputDirectory: temporary },
      { generators: ['events', 'props'] }
    )
  )
    throw new Error('react-native-svg codegen failed');
  // The props generator also emits Java; this host consumes the C++ only.
  const files = fs
    .readdirSync(temporary, { withFileTypes: true })
    .filter((entry) => entry.isFile() && /\.(cpp|h)$/.test(entry.name))
    .map((entry) => entry.name)
    .sort();
  /** @param {string} name */
  const generatedContent = (name) =>
    fs.readFileSync(path.join(temporary, name), 'utf8').replace(/[\t ]+$/gm, '');
  if (mode === '--write') {
    fs.mkdirSync(output, { recursive: true });
    for (const name of fs.readdirSync(output)) {
      if (!files.includes(name)) fs.rmSync(path.join(output, name));
    }
    for (const name of files) fs.writeFileSync(path.join(output, name), generatedContent(name));
  } else {
    const actual = fs.existsSync(output) ? fs.readdirSync(output).sort() : [];
    if (JSON.stringify(files) !== JSON.stringify(actual))
      throw new Error('react-native-svg generated file set is stale');
    for (const name of files) {
      if (generatedContent(name) !== fs.readFileSync(path.join(output, name), 'utf8')) {
        throw new Error(`react-native-svg generated source is stale: ${name}`);
      }
    }
  }
  console.log(
    `react-native-svg Fabric codegen ${mode.slice(2)} passed (${svgVersion}, ${files.length} files)`
  );
} finally {
  fs.rmSync(temporary, { recursive: true, force: true });
}
