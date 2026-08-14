#!/usr/bin/env node

import { readdir, readFile } from 'node:fs/promises';
import path from 'node:path';

const packagesDir = path.join(process.cwd(), 'packages');
const appsDir = path.join(process.cwd(), 'apps');

/**
 * @param {string} dir
 */
async function validate(dir) {
  const entries = await readdir(dir, { withFileTypes: true });
  let manifests = 0;
  let scaffolds = 0;
  for (const entry of entries) {
    if (!entry.isDirectory()) continue;
    const pkgJsonPath = path.join(dir, entry.name, 'package.json');
    try {
      const content = await readFile(pkgJsonPath, 'utf8');
      const pkg = JSON.parse(content);
      if (!pkg.name) {
        throw new Error(`${pkgJsonPath} is missing "name"`);
      }
      if (!pkg.version && pkg.private !== true) {
        throw new Error(`${pkgJsonPath} must declare "version" or be private`);
      }
      if (pkg.scripts != null && (typeof pkg.scripts !== 'object' || Array.isArray(pkg.scripts))) {
        throw new Error(`${pkgJsonPath} has an invalid "scripts" field`);
      }
      manifests += 1;
      console.log(`OK: ${pkg.name}`);
    } catch (err) {
      if (
        err instanceof Error &&
        'code' in err &&
        /** @type {NodeJS.ErrnoException} */ (err).code === 'ENOENT'
      ) {
        scaffolds += 1;
        console.log(`SCAFFOLD: ${path.relative(process.cwd(), path.dirname(pkgJsonPath))}`);
      } else {
        throw err;
      }
    }
  }
  return { manifests, scaffolds };
}

async function main() {
  const packages = await validate(packagesDir);
  const apps = await validate(appsDir);
  console.log(
    `validate:manifests passed (${packages.manifests + apps.manifests} manifests, ` +
      `${packages.scaffolds + apps.scaffolds} scaffold-only directories).`
  );
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
