#!/usr/bin/env node
import { syncDocs } from './lib/docs-model.mjs';

const args = process.argv.slice(2);
if (
  args.some((arg) => !['--write', '--release'].includes(arg)) ||
  (args.includes('--write') && args.includes('--release'))
) {
  console.error('Usage: node scripts/validate-docs.mjs [--write | --release]');
  process.exitCode = 1;
} else {
  try {
    const result = await syncDocs(
      process.cwd(),
      args.includes('--write'),
      args.includes('--release')
    );
    console.log(
      `Documentation ${args.includes('--write') ? 'updated' : 'validated'}: ${result.components} components, ${result.gates} release gates.`
    );
  } catch (error) {
    console.error(error instanceof Error ? error.message : error);
    process.exitCode = 1;
  }
}
