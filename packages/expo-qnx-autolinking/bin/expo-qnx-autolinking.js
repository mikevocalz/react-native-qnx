#!/usr/bin/env node

'use strict';

const fs = require('fs');
const path = require('path');
const { findQnxExpoModules, generateQnxCMake } = require('../src');

function valueAfter(args, name) {
  const index = args.indexOf(name);
  return index >= 0 ? args[index + 1] : undefined;
}

async function main() {
  const args = process.argv.slice(2);
  const projectRoot = path.resolve(valueAfter(args, '--project-root') || process.cwd());
  const jsonOutput = valueAfter(args, '--json');
  const cmakeOutput = valueAfter(args, '--cmake');

  if (!jsonOutput && !cmakeOutput) {
    throw new Error(
      'Usage: expo-qnx-autolinking --project-root <root> [--json file] [--cmake file]'
    );
  }

  const descriptors = await findQnxExpoModules(projectRoot);

  if (jsonOutput) {
    const file = path.resolve(jsonOutput);
    fs.mkdirSync(path.dirname(file), { recursive: true });
    fs.writeFileSync(
      file,
      `${JSON.stringify({ schemaVersion: 1, platform: 'qnx', modules: descriptors }, null, 2)}\n`
    );
  }

  if (cmakeOutput) {
    const file = path.resolve(cmakeOutput);
    fs.mkdirSync(path.dirname(file), { recursive: true });
    fs.writeFileSync(file, generateQnxCMake(descriptors));
  }

  console.log(`Resolved ${descriptors.length} Expo module(s) for QNX.`);
}

main().catch((error) => {
  console.error(error instanceof Error ? error.message : error);
  process.exitCode = 1;
});
