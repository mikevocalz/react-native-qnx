#!/usr/bin/env node
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
if (version !== '0.88.1') throw new Error(`Re-audit Qt Video generation for codegen ${version}`);
const { TypeScriptParser } = require(path.join(codegenRoot, 'lib/parsers/typescript/parser.js'));
const { generate } = require(path.join(codegenRoot, 'lib/generators/RNCodegen.js'));
const mode = process.argv[2];
if (!['--write', '--check'].includes(mode) || process.argv.length !== 3) {
  throw new Error('Usage: node scripts/qt-video-codegen.mjs --write|--check');
}
const spec = path.join(root, 'packages/react-native-qnx/qt/QtVideoNativeComponent.ts');
const output = path.join(root, 'vnext/generated/react/renderer/components/RnqnxQtVideo');
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'rnqnx-qt-video-'));
try {
  const schema = new TypeScriptParser().parseFile(spec);
  if (
    !generate(
      { libraryName: 'RnqnxQtVideo', schema, outputDirectory: temporary },
      {
        generators: ['descriptors', 'events', 'props', 'states', 'shadow-nodes'],
      }
    )
  )
    throw new Error('React Native Video codegen failed');
  // The upstream props generator also emits Java. This host consumes its
  // C++ Fabric outputs only; Android registration is not a QNX backend.
  const files = fs
    .readdirSync(temporary, { withFileTypes: true })
    .filter((entry) => entry.isFile() && /\.(cpp|h)$/.test(entry.name))
    .map((entry) => entry.name)
    .sort();
  // Keep upstream output deterministic while removing trailing horizontal
  // whitespace that would otherwise fail the repository's diff checks.
  /** @param {string} name */
  const generatedContent = (name) =>
    fs.readFileSync(path.join(temporary, name), 'utf8').replace(/[\t ]+$/gm, '');
  if (mode === '--write') {
    fs.mkdirSync(output, { recursive: true });
    for (const name of fs.readdirSync(output)) {
      if (!files.includes(name)) throw new Error(`Unexpected generated file: ${name}`);
    }
    for (const name of files) fs.writeFileSync(path.join(output, name), generatedContent(name));
  } else {
    const actual = fs.existsSync(output) ? fs.readdirSync(output).sort() : [];
    if (JSON.stringify(files) !== JSON.stringify(actual))
      throw new Error('Qt Video generated file set is stale');
    for (const name of files) {
      if (generatedContent(name) !== fs.readFileSync(path.join(output, name), 'utf8')) {
        throw new Error(`Qt Video generated source is stale: ${name}`);
      }
    }
  }
  console.log(
    `Qt Video Fabric codegen ${mode.slice(2)} passed (${version}, ${files.length} files)`
  );
} finally {
  fs.rmSync(temporary, { recursive: true, force: true });
}
