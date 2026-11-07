#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const { parseArgs } = require('node:util');
const { findExpoModulesForPlatform, generateExpoPlatformCMake } = require('../src');

async function main() {
  const { values, tokens } = parseArgs({
    args: process.argv.slice(2), strict: true, allowPositionals: false, tokens: true,
    options: {
      'project-root': { type: 'string' }, platform: { type: 'string' },
      json: { type: 'string' }, cmake: { type: 'string' }, help: { type: 'boolean' },
    },
  });
  const seen = new Set();
  for (const token of tokens) {
    if (token.kind !== 'option') continue;
    if (seen.has(token.name)) throw new Error(`Duplicate option --${token.name}.`);
    seen.add(token.name);
  }
  const usage = 'expo-qnx-autolinking --project-root <root> [--platform qnx|marathon] [--json file] [--cmake file]';
  if (values.help) { console.log(usage); return; }
  const platform = values.platform ?? 'qnx';
  if (!['qnx', 'marathon'].includes(platform)) throw new Error('Use --platform qnx or marathon.');
  for (const name of ['project-root', 'json', 'cmake']) {
    if (values[name] !== undefined && !values[name].trim()) throw new Error(`--${name} must not be empty.`);
  }
  if (!values.json && !values.cmake) throw new Error(`Usage: ${usage}`);
  const projectRoot = path.resolve(values['project-root'] ?? process.cwd());
  if (values.json && values.cmake && path.resolve(values.json) === path.resolve(values.cmake)) {
    throw new Error('JSON and CMake outputs must be different files.');
  }
  const descriptors = await findExpoModulesForPlatform(projectRoot, platform);
  // Validate/generate both outputs before writing either. This low-level
  // generator preserves the existing overwrite behavior for owned build files.
  const cmake = generateExpoPlatformCMake(descriptors, platform);
  if (values.json) {
    const file = path.resolve(values.json);
    fs.mkdirSync(path.dirname(file), { recursive: true });
    fs.writeFileSync(file, `${JSON.stringify({ schemaVersion: 1, platform, modules: descriptors }, null, 2)}\n`);
  }
  if (values.cmake) {
    const file = path.resolve(values.cmake);
    fs.mkdirSync(path.dirname(file), { recursive: true });
    fs.writeFileSync(file, cmake);
  }
  console.log(`Resolved ${descriptors.length} Expo module(s) for ${platform}.`);
}
main().catch((error) => {
  console.error(error instanceof Error ? error.message : error);
  process.exitCode = 1;
});
