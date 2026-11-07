#!/usr/bin/env node
import { main } from './cli.mjs';
import { projectMain } from './project.mjs';

try {
  const args = process.argv.slice(2);
  process.exitCode = args[0] === 'project' ? await projectMain(args.slice(1)) : await main(args);
  if (args.length === 0 || args[0] === '--help' || args[0] === '-h') {
    console.log('\nExternal apps: rnqnx project --help (bundling only; no native provisioning).');
  }
} catch (error) {
  console.error(`rnqnx: ${error instanceof Error ? error.message : error}`);
  process.exitCode = 1;
}
