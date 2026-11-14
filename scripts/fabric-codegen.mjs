#!/usr/bin/env node
// Generates the C++ Fabric descriptors, props, events, states and shadow nodes
// for a third-party library's codegen specs with the React Native codegen the
// repository already pins (0.88.1). Output goes to a build directory; nothing
// generated from an installed package is committed.
//
//   node scripts/fabric-codegen.mjs <libraryName> <outputDir> <spec.ts>...
//
// <libraryName> becomes the component directory, e.g. "rnreanimated" gives
// react/renderer/components/rnreanimated/Props.h under <outputDir>.
import { createRequire } from 'node:module';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const [libraryName, outputDir, ...specs] = process.argv.slice(2);
if (!libraryName || !outputDir || specs.length === 0) {
  throw new Error('Usage: fabric-codegen.mjs <libraryName> <outputDir> <spec.ts>...');
}
const require = createRequire(path.join(process.cwd(), 'package.json'));
const rnRequire = createRequire(require.resolve('react-native/package.json'));
const codegenRoot = path.dirname(rnRequire.resolve('@react-native/codegen/package.json'));
const version = JSON.parse(fs.readFileSync(path.join(codegenRoot, 'package.json'), 'utf8')).version;
if (version !== '0.88.1') throw new Error(`Re-audit Fabric generation for codegen ${version}`);
const { TypeScriptParser } = require(path.join(codegenRoot, 'lib/parsers/typescript/parser.js'));
const { generate } = require(path.join(codegenRoot, 'lib/generators/RNCodegen.js'));

// One schema per library: merge the modules of every spec file.
const parser = new TypeScriptParser();
const schema = { modules: {} };
for (const spec of specs) {
  const parsed = parser.parseFile(spec);
  Object.assign(schema.modules, parsed.modules);
}
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'rnqnx-fabric-'));
try {
  if (
    !generate(
      { libraryName, schema, outputDirectory: temporary },
      { generators: ['descriptors', 'events', 'props', 'states', 'shadow-nodes'] }
    )
  ) {
    throw new Error(`Fabric codegen failed for ${libraryName}`);
  }
  const target = path.join(outputDir, 'react/renderer/components', libraryName);
  fs.rmSync(target, { recursive: true, force: true });
  fs.mkdirSync(target, { recursive: true });
  const files = fs
    .readdirSync(temporary, { withFileTypes: true })
    .filter((entry) => entry.isFile() && /\.(cpp|h)$/.test(entry.name))
    .map((entry) => entry.name)
    .sort();
  for (const name of files) {
    fs.writeFileSync(
      path.join(target, name),
      fs.readFileSync(path.join(temporary, name), 'utf8').replace(/[\t ]+$/gm, '')
    );
  }
  console.log(`${libraryName}: ${files.length} files in ${target}`);
} finally {
  fs.rmSync(temporary, { recursive: true, force: true });
}
