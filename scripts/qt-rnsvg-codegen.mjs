#!/usr/bin/env node
// Generates the Fabric C++ props/events for the react-native-svg components
// this host mounts, from the pinned package's own TypeScript specs. Same
// determinism contract as qt-svg-codegen.mjs: --write refreshes, --check fails
// on drift. Shadow nodes and descriptors come from react-native-svg's own
// common/cpp sources, so only props, events and the registration list are
// generated here.
//
// Three things differ from the package's own specs, all applied to a patched
// copy of the spec text (the installed package is never edited):
//   - the gradient stop array is Double, not Float (packed ARGB colors);
//   - clipRule defaults to -1 so "unset" and "evenodd" (0) differ;
//   - spreadMethod (gradients) and colorInterpolationFilters (filter primitives)
//     exist on the components that lack them, and the filter primitives the package declares in JavaScript
//     but never mounts get Fabric specs of their own
//     (packages/react-native-qnx/svg/fabric). The JavaScript half lives in
//     packages/react-native-qnx/svg and is applied by Metro on QNX only.
//
// React Native sends a prop only if the component's JavaScript view config
// lists it, and Metro builds that config from the spec file. So a patch that
// adds a prop (patches marked `js` below) also needs the patched spec on the
// JavaScript side: those specs are written to
// packages/react-native-qnx/svg/overlay/fabric, which Metro serves in place of
// the package's copy.
import { createRequire } from 'node:module';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const require = createRequire(path.join(root, 'package.json'));
const prettier = require('prettier');
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
// Filter primitives react-native-svg 15.16.0 never mounts. Their specs are
// ours and follow the package's own FilterPrimitiveCommonProps layout.
const extraSpecRoot = path.join(root, 'packages/react-native-qnx/svg/fabric');
const extraSpecs = [
  'FeComponentTransferNativeComponent',
  'FeConvolveMatrixNativeComponent',
  'FeDiffuseLightingNativeComponent',
  'FeDisplacementMapNativeComponent',
  'FeDropShadowNativeComponent',
  'FeImageNativeComponent',
  'FeMorphologyNativeComponent',
  'FeSpecularLightingNativeComponent',
  'FeTileNativeComponent',
  'FeTurbulenceNativeComponent',
];

const colorInterpolationProp = [
  '  colorInterpolationFilters?: CodegenTypes.WithDefault<',
  "    'auto' | 'sRGB' | 'linearRGB',",
  "    'auto'",
  '  >;',
].join('\n');
const gradientSpecs = /^(Linear|Radial)GradientNativeComponent$/;
const filterPrimitiveSpecs =
  /^Fe(Blend|ColorMatrix|Composite|Flood|GaussianBlur|Merge|Offset)NativeComponent$/;

/**
 * Text edits applied to a spec before it is parsed. `count` is how many times
 * `from` must occur: zero means the package changed shape and the patch no
 * longer applies, which must stop the build rather than skip silently.
 * `js` marks a patch that adds a prop the JavaScript view config must list.
 * @type {Array<{ name: RegExp, from: string, to: string, count: number, js?: boolean, why: string }>}
 */
const patches = [
  {
    name: gradientSpecs,
    from: 'gradient?: ReadonlyArray<CodegenTypes.Float>;',
    to: 'gradient?: ReadonlyArray<CodegenTypes.Double>;',
    count: 1,
    why: "Gradient stop colors are packed ARGB integers carried in this array. React Native's Float is 32-bit on this host, which cannot hold a packed color that has any alpha below 255 (blue would be rounded away).",
  },
  {
    name: /^(?!IOSSvgView|Android|Fe|Filter)\w+NativeComponent$/,
    from: 'clipRule?: CodegenTypes.WithDefault<CodegenTypes.Int32, 0>;',
    to: 'clipRule?: CodegenTypes.WithDefault<CodegenTypes.Int32, -1>;',
    count: 1,
    why: 'The package maps evenodd to 0 and nonzero to 1 and leaves the prop alone when clipRule is unset, so the default 0 made "unset" read as evenodd.',
  },
  {
    name: gradientSpecs,
    from: '  gradientTransform?: ReadonlyArray<CodegenTypes.Float> | null;',
    to: [
      '  gradientTransform?: ReadonlyArray<CodegenTypes.Float> | null;',
      '  spreadMethod?: CodegenTypes.WithDefault<CodegenTypes.Int32, 0>;',
    ].join('\n'),
    count: 1,
    js: true,
    why: 'spreadMethod is accepted by the TypeScript types but has no native prop. 0 pad, 1 reflect, 2 repeat.',
  },
  {
    name: filterPrimitiveSpecs,
    from: '  result?: string;\n}',
    to: `  result?: string;\n${colorInterpolationProp}\n}`,
    count: 1,
    js: true,
    why: "color-interpolation-filters is not sent by the package. `auto` means unset: the JavaScript Filter copies its own value onto primitives that have none, and the bridge reads `auto` as SVG's default, linearRGB.",
  },
];

/**
 * @param {string} name spec file name without extension
 * @param {string} source
 * @returns {{ text: string, js: boolean }} `js`: a patch changed the JavaScript view config
 */
function applyPatches(name, source) {
  let text = source;
  let js = false;
  for (const patch of patches) {
    if (!patch.name.test(name)) continue;
    const parts = text.split(patch.from);
    if (parts.length - 1 !== patch.count) {
      throw new Error(
        `${name}: expected ${patch.count} occurrence(s) of "${patch.from}", found ${parts.length - 1}. ${patch.why}`
      );
    }
    text = parts.join(patch.to);
    js ||= patch.js === true;
  }
  // FeMerge and FeOffset do not import CodegenTypes; the added prop needs it.
  if (/CodegenTypes\./.test(text) && !/import[^;]*\bCodegenTypes\b/.test(text)) {
    text = `import type { CodegenTypes } from 'react-native';\n${text}`;
  }
  return { text, js };
}

/**
 * The patched package spec as a file that can live outside the package: its
 * relative imports become package specifiers, which Metro resolves to the
 * package's own files because imports from the overlay are never redirected.
 * @param {string} name
 * @param {string} text
 */
async function overlaySpec(name, text) {
  const rewritten = text
    .replace(/from '\.\/([\w]+)'/g, "from 'react-native-svg/src/fabric/$1'")
    .replace(/from '\.\.\/(lib\/[\w/]+)'/g, "from 'react-native-svg/src/$1'");
  const left = rewritten.match(/from '\.[^']*'/g);
  if (left) throw new Error(`${name}: relative import left in overlay spec: ${left.join(', ')}`);
  const header = `// Generated by scripts/qt-rnsvg-codegen.mjs from react-native-svg ${svgVersion}.\n// Do not edit: run pnpm qt:rnsvg:generate.\n`;
  const file = path.join(overlayOutput, `${name}.ts`);
  const options = (await prettier.resolveConfig(file)) ?? {};
  return prettier.format(header + rewritten, { ...options, parser: 'typescript' });
}

const output = path.join(root, 'vnext/generated/react/renderer/components/rnsvg');
const overlayOutput = path.join(root, 'packages/react-native-qnx/svg/overlay/fabric');
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'rnqnx-rnsvg-'));
try {
  const parser = new TypeScriptParser();
  const modules = {};
  const specDirectory = path.join(temporary, 'spec');
  fs.mkdirSync(specDirectory);
  const sources = [
    ...specs.map((name) => ({ name, file: path.join(svgRoot, 'src/fabric', `${name}.ts`) })),
    ...extraSpecs.map((name) => ({ name, file: path.join(extraSpecRoot, `${name}.ts`) })),
  ];
  /** @type {Map<string, string>} */
  const overlaySpecs = new Map();
  for (const { name, file } of sources) {
    const source = fs.readFileSync(file, 'utf8');
    const patched = applyPatches(name, source);
    const patchedFile = path.join(specDirectory, `${name}.ts`);
    fs.writeFileSync(patchedFile, patched.text);
    if (patched.js && !extraSpecs.includes(name)) {
      overlaySpecs.set(`${name}.ts`, await overlaySpec(name, patched.text));
    }
    const schema = parser.parseFile(patchedFile);
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
  /**
   * Writes `expected` (file name to content) into `directory`, or checks that
   * the directory holds exactly that.
   * @param {string} directory
   * @param {Map<string, string>} expected
   * @param {string} what
   */
  const sync = (directory, expected, what) => {
    if (mode === '--write') {
      fs.mkdirSync(directory, { recursive: true });
      for (const name of fs.readdirSync(directory)) {
        if (!expected.has(name)) fs.rmSync(path.join(directory, name));
      }
      for (const [name, content] of expected) fs.writeFileSync(path.join(directory, name), content);
      return;
    }
    const actual = fs.existsSync(directory) ? fs.readdirSync(directory).sort() : [];
    if (JSON.stringify([...expected.keys()].sort()) !== JSON.stringify(actual))
      throw new Error(`${what} generated file set is stale`);
    for (const [name, content] of expected) {
      if (content !== fs.readFileSync(path.join(directory, name), 'utf8')) {
        throw new Error(`${what} generated source is stale: ${name}`);
      }
    }
  };
  sync(output, new Map(files.map((name) => [name, generatedContent(name)])), 'react-native-svg');
  sync(overlayOutput, overlaySpecs, 'react-native-svg overlay spec');
  console.log(
    `react-native-svg Fabric codegen ${mode.slice(2)} passed (${svgVersion}, ${files.length} files, ` +
      `${overlaySpecs.size} overlay specs)`
  );
} finally {
  fs.rmSync(temporary, { recursive: true, force: true });
}
