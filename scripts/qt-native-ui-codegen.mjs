#!/usr/bin/env node
// Generates the Fabric C++ props, events, shadow nodes and descriptors for the
// Qt native UI components (drawer, tab bar, ExpoImage view) from their
// TypeScript specs in packages/react-native-qnx/qt. Same determinism contract
// as qt-svg-codegen.mjs: --write refreshes, --check fails on drift.
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
  throw new Error(`Re-audit Qt native UI generation for codegen ${version}`);
const { TypeScriptParser } = require(path.join(codegenRoot, 'lib/parsers/typescript/parser.js'));
const { generate } = require(path.join(codegenRoot, 'lib/generators/RNCodegen.js'));
const mode = process.argv[2];
if (!['--write', '--check'].includes(mode) || process.argv.length !== 3) {
  throw new Error('Usage: node scripts/qt-native-ui-codegen.mjs --write|--check');
}

const libraries = [
  { name: 'RnqnxQtDrawer', spec: 'RNQnxDrawerNativeComponent.ts' },
  { name: 'RnqnxQtTabBar', spec: 'RNQnxTabBarNativeComponent.ts' },
  { name: 'RnqnxExpoImage', spec: 'ExpoImageNativeComponent.ts' },
];
const outputRoot = path.join(root, 'vnext/generated/react/renderer/components');
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'rnqnx-qt-native-ui-'));
let failures = 0;
try {
  for (const library of libraries) {
    const spec = path.join(root, 'packages/react-native-qnx/qt', library.spec);
    const scratch = path.join(temporary, library.name);
    fs.mkdirSync(scratch);
    const schema = new TypeScriptParser().parseFile(spec);
    if (
      !generate(
        { libraryName: library.name, schema, outputDirectory: scratch },
        { generators: ['descriptors', 'events', 'props', 'states', 'shadow-nodes'] }
      )
    )
      throw new Error(`Codegen failed for ${library.name}`);
    // Only C++ is consumed; the props generator also emits Java.
    const files = fs
      .readdirSync(scratch, { withFileTypes: true })
      .filter((entry) => entry.isFile() && /\.(cpp|h)$/.test(entry.name))
      .map((entry) => entry.name)
      .sort();
    /** @param {string} name */
    const content = (name) =>
      fs.readFileSync(path.join(scratch, name), 'utf8').replace(/[\t ]+$/gm, '');
    const output = path.join(outputRoot, library.name);
    if (mode === '--write') {
      fs.mkdirSync(output, { recursive: true });
      for (const name of fs.readdirSync(output)) {
        if (!files.includes(name)) fs.rmSync(path.join(output, name));
      }
      for (const name of files) fs.writeFileSync(path.join(output, name), content(name));
    } else {
      const actual = fs.existsSync(output) ? fs.readdirSync(output).sort() : [];
      if (JSON.stringify(files) !== JSON.stringify(actual)) {
        console.error(`${library.name}: generated file set is stale`);
        failures += 1;
        continue;
      }
      for (const name of files) {
        if (content(name) !== fs.readFileSync(path.join(output, name), 'utf8')) {
          console.error(`${library.name}: generated source is stale: ${name}`);
          failures += 1;
        }
      }
    }
  }
  if (failures > 0) throw new Error('Qt native UI generated sources are stale');
  console.log(
    `Qt native UI Fabric codegen ${mode.slice(2)} passed (${version}, ${libraries.length} libraries)`
  );
} finally {
  fs.rmSync(temporary, { recursive: true, force: true });
}
