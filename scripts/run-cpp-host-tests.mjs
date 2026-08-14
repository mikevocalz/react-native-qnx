#!/usr/bin/env node

import { spawn } from 'node:child_process';

const preset =
  process.platform === 'darwin'
    ? 'macos-host-tests'
    : process.platform === 'linux'
      ? 'linux-host-tests'
      : null;

if (preset == null) {
  console.error(`Host C++ tests are not configured for ${process.platform}.`);
  process.exitCode = 1;
} else {
  await run('cmake', ['--fresh', '--preset', preset]);
  await run('cmake', ['--build', '--preset', preset]);
  await run('ctest', ['--preset', preset]);
}

/**
 * @param {string} command
 * @param {string[]} args
 * @returns {Promise<void>}
 */
function run(command, args) {
  return new Promise((resolve, reject) => {
    const child = spawn(command, args, { stdio: 'inherit' });
    child.once('error', reject);
    child.once('exit', (code, signal) => {
      if (code === 0) {
        resolve();
      } else {
        reject(new Error(`${command} failed (${signal ?? `exit ${code}`})`));
      }
    });
  });
}
