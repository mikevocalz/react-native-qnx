#!/usr/bin/env node

import { readdir } from 'node:fs/promises';
import path from 'node:path';

const packageRoot = path.join(process.cwd(), 'packages');

/**
 * @param {string} directory
 * @returns {Promise<string[]>}
 */
async function findNitroConfigs(directory) {
  const entries = await readdir(directory, { withFileTypes: true });
  const configs = [];
  for (const entry of entries) {
    const fullPath = path.join(directory, entry.name);
    if (entry.isDirectory()) {
      configs.push(...(await findNitroConfigs(fullPath)));
    } else if (entry.isFile() && entry.name === 'nitro.json') {
      configs.push(fullPath);
    }
  }
  return configs;
}

async function main() {
  const configs = await findNitroConfigs(packageRoot);
  if (configs.length === 0) {
    console.log(
      'No nitro.json files exist yet; Nitrogen generation is not applicable to this milestone.'
    );
    return;
  }

  throw new Error(
    `Found ${configs.length} Nitro configuration(s), but deterministic generated-output checking ` +
      'has not been implemented. Add generation plus a clean-diff assertion before accepting specs.'
  );
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
