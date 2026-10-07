#!/usr/bin/env node

import { createHash } from 'node:crypto';
import { readdir, readFile, stat, writeFile } from 'node:fs/promises';
import path from 'node:path';

const root = process.cwd();
const artifactRoot = path.join(root, 'artifacts', 'pi');

/** @param {string | undefined} argument @returns {Promise<string>} */
async function resolveEvidenceDir(argument) {
  if (argument) {
    return path.resolve(argument);
  }

  let entries;
  try {
    entries = await readdir(artifactRoot, { withFileTypes: true });
  } catch (error) {
    if (
      error instanceof Error &&
      'code' in error &&
      /** @type {NodeJS.ErrnoException} */ (error).code === 'ENOENT'
    ) {
      throw new Error(
        'No artifacts/pi directory exists. Run pnpm pi:install on the QNX target first.'
      );
    }
    throw error;
  }

  const dirs = entries
    .filter((entry) => entry.isDirectory())
    .map((entry) => entry.name)
    .sort();

  if (dirs.length === 0) {
    throw new Error('No Pi evidence directories exist under artifacts/pi.');
  }

  const latest = dirs.at(-1);
  if (!latest) {
    throw new Error('No Pi evidence directories exist under artifacts/pi.');
  }
  return path.join(artifactRoot, latest);
}

/** @param {string} directory @param {string} relativePath @returns {Promise<string>} */
async function requireFile(directory, relativePath) {
  const absolute = path.join(directory, relativePath);
  const info = await stat(absolute).catch((error) => {
    if (
      error instanceof Error &&
      'code' in error &&
      /** @type {NodeJS.ErrnoException} */ (error).code === 'ENOENT'
    ) {
      throw new Error(`Missing required evidence file: ${relativePath}`);
    }
    throw error;
  });
  if (!info.isFile() || info.size === 0) {
    throw new Error(`Evidence file is empty or not a file: ${relativePath}`);
  }
  return absolute;
}

/** @param {string} text @param {RegExp} expression @param {string} label */
function assertContains(text, expression, label) {
  if (!expression.test(text)) {
    throw new Error(`Evidence check failed: ${label}`);
  }
}

/** @param {string} text @param {RegExp} expression @returns {number} */
function countMatches(text, expression) {
  return text.match(expression)?.length ?? 0;
}

/** @typedef {{ absolute: string, relative: string }} EvidenceFile */
/** @param {string} directory @param {string} [base] @returns {Promise<EvidenceFile[]>} */
async function walkFiles(directory, base = directory) {
  const entries = await readdir(directory, { withFileTypes: true });
  const files = [];
  for (const entry of entries) {
    const absolute = path.join(directory, entry.name);
    if (entry.isDirectory()) {
      files.push(...(await walkFiles(absolute, base)));
    } else if (entry.isFile()) {
      const relative = path.relative(base, absolute).split(path.sep).join('/');
      if (relative !== 'manifest.json') {
        files.push({ absolute, relative });
      }
    }
  }
  return files.sort((a, b) => a.relative.localeCompare(b.relative));
}

/** @param {string} absolute @returns {Promise<{ bytes: number, sha256: string }>} */
async function hashFile(absolute) {
  const data = await readFile(absolute);
  return {
    bytes: data.byteLength,
    sha256: createHash('sha256').update(data).digest('hex'),
  };
}

async function main() {
  const evidenceDir = await resolveEvidenceDir(process.argv[2]);

  const required = [
    'completed-at.txt',
    'config.txt',
    'playground-output.txt',
    'slog2-tail.txt',
    'target.txt',
    'preflight/qcc-V.txt',
    'preflight/qxx-V.txt',
    'preflight/target-preflight.txt',
  ];

  for (const file of required) {
    await requireFile(evidenceDir, file);
  }

  const [output, preflight, qcc, completedAt, config, target] = await Promise.all([
    readFile(path.join(evidenceDir, 'playground-output.txt'), 'utf8'),
    readFile(path.join(evidenceDir, 'preflight/target-preflight.txt'), 'utf8'),
    readFile(path.join(evidenceDir, 'preflight/qcc-V.txt'), 'utf8'),
    readFile(path.join(evidenceDir, 'completed-at.txt'), 'utf8'),
    readFile(path.join(evidenceDir, 'config.txt'), 'utf8'),
    readFile(path.join(evidenceDir, 'target.txt'), 'utf8'),
  ]);

  let board = 'unknown';
  try {
    board = (await readFile(path.join(evidenceDir, 'board.txt'), 'utf8')).trim();
  } catch (error) {
    if (!(
      error instanceof Error &&
      'code' in error &&
      /** @type {NodeJS.ErrnoException} */ (error).code === 'ENOENT'
    )) {
      throw error;
    }
  }
  assertContains(board, /^(unknown|pi4|pi5|pi400)$/, 'Raspberry Pi board label');

  let runCount = 1;
  try {
    const runCountText = await readFile(path.join(evidenceDir, 'run-count.txt'), 'utf8');
    const runCountValue = runCountText.trim();
    if (!/^\d+$/.test(runCountValue)) {
      throw new Error(
        'Evidence check failed: run-count.txt must contain an integer from 1 through 20'
      );
    }
    runCount = Number(runCountValue);
    if (!Number.isSafeInteger(runCount) || runCount < 1 || runCount > 20) {
      throw new Error(
        'Evidence check failed: run-count.txt must contain an integer from 1 through 20'
      );
    }
  } catch (error) {
    if (!(
      error instanceof Error &&
      'code' in error &&
      /** @type {NodeJS.ErrnoException} */ (error).code === 'ENOENT'
    )) {
      throw error;
    }
  }

  const screenInitCount = countMatches(
    output,
    /rnqnx_playground: Screen surface initialized \([0-9]+x[0-9]+\)/g
  );
  const cleanExitCount = countMatches(
    output,
    /rnqnx_playground: exiting cleanly after [0-9]+ frames/g
  );
  if (screenInitCount !== runCount) {
    throw new Error(
      `Evidence check failed: expected ${runCount} Screen initialization proof line(s), found ${screenInitCount}`
    );
  }
  if (cleanExitCount !== runCount) {
    throw new Error(
      `Evidence check failed: expected ${runCount} clean shutdown proof line(s), found ${cleanExitCount}`
    );
  }
  assertContains(preflight, /^machine=aarch64le$/m, 'target reports aarch64le');
  assertContains(preflight, /^screen=ready$/m, 'QNX Screen reports ready');
  assertContains(preflight, /^target_dir=writable$/m, 'deployment directory is writable');
  assertContains(qcc, /gcc_ntoaarch64le/, 'QNX compiler advertises AArch64le');
  assertContains(
    completedAt.trim(),
    /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/,
    'completion timestamp is UTC ISO-8601'
  );
  assertContains(config.trim(), /^(Debug|Release)$/, 'configuration is Debug or Release');
  assertContains(target.trim(), /^[A-Za-z_][A-Za-z0-9._-]*@[A-Za-z0-9.-]+$/, 'target identity');

  const screenshotPath = path.join(evidenceDir, 'rnqnx-playground.bmp');
  let screenshotCaptured = false;
  try {
    const screenshotInfo = await stat(screenshotPath);
    screenshotCaptured = screenshotInfo.isFile() && screenshotInfo.size > 0;
  } catch (error) {
    if (!(
      error instanceof Error &&
      'code' in error &&
      /** @type {NodeJS.ErrnoException} */ (error).code === 'ENOENT'
    )) {
      throw error;
    }
  }

  const files = await walkFiles(evidenceDir);
  /** @type {Record<string, { bytes: number, sha256: string }>} */
  const inventory = {};
  for (const file of files) {
    inventory[file.relative] = await hashFile(file.absolute);
  }

  const manifest = {
    schemaVersion: 1,
    generatedAt: new Date().toISOString(),
    evidenceDirectory: path.relative(root, evidenceDir).split(path.sep).join('/'),
    target: target.trim(),
    board,
    configuration: config.trim(),
    runCount,
    completedAt: completedAt.trim(),
    checks: {
      aarch64le: true,
      screenReady: true,
      targetDirectoryWritable: true,
      screenInitialized: true,
      cleanShutdown: true,
      repeatedRuns: runCount > 1,
      qccAarch64Variant: true,
      boardRecorded: board !== 'unknown',
      screenshotCaptured,
    },
    files: inventory,
  };

  const manifestPath = path.join(evidenceDir, 'manifest.json');
  await writeFile(manifestPath, `${JSON.stringify(manifest, null, 2)}\n`);

  console.log('Pi evidence verified.');
  console.log(`Directory: ${evidenceDir}`);
  console.log(`Target: ${manifest.target}`);
  console.log(`Board: ${manifest.board}`);
  console.log(`Configuration: ${manifest.configuration}`);
  console.log(`Validated runs: ${manifest.runCount}`);
  console.log(`Screenshot: ${screenshotCaptured ? 'captured' : 'not available'}`);
  console.log(`Manifest: ${manifestPath}`);
}

main().catch((error) => {
  console.error(error instanceof Error ? error.message : error);
  process.exitCode = 1;
});
