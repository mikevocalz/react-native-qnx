#!/usr/bin/env node

import { access, readdir } from 'node:fs/promises';
import { spawn } from 'node:child_process';
import path from 'node:path';

const packagesDir = path.join(process.cwd(), 'packages');

async function main() {
  const entries = await readdir(packagesDir, { withFileTypes: true });
  const packages = entries.filter((e) => e.isDirectory()).map((e) => e.name);
  const buildable = [];

  for (const packageName of packages) {
    const manifest = path.join(packagesDir, packageName, 'package.json');
    try {
      await access(manifest);
      buildable.push(packageName);
    } catch (error) {
      if (!(error instanceof Error && 'code' in error && error.code === 'ENOENT')) {
        throw error;
      }
    }
  }

  if (buildable.length === 0) {
    console.log('No package manifests exist yet; package build is not applicable.');
    return;
  }

  console.log(`Building ${buildable.length} workspace package(s) with declared build scripts...`);
  await runWorkspaceBuild();
}

/** @returns {Promise<void>} */
function runWorkspaceBuild() {
  return new Promise((resolve, reject) => {
    const child = spawn('pnpm', ['--recursive', '--if-present', 'run', 'build'], {
      cwd: process.cwd(),
      stdio: 'inherit',
    });
    child.once('error', reject);
    child.once('exit', (code, signal) => {
      if (code === 0) {
        resolve();
      } else {
        reject(new Error(`Workspace package build failed (${signal ?? `exit ${code}`})`));
      }
    });
  });
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
