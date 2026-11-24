#!/usr/bin/env node
// Create the DRAFT GitHub Release for a packed runner (ADR-0018, step 3).
//
// Default is a dry run: it checks the packed directory, prints each asset's
// SHA-256 and the exact `gh release create` command, and changes nothing.
// Running the command needs both:
//   1. the --confirm flag, and
//   2. RNQNX_RELEASE_OWNER_APPROVED set to the exact release tag.
// The owner sets the second one. Nothing here publishes a draft, pushes a tag,
// publishes to npm or runs the attestation workflow.
//
// Usage: node scripts/publish-runner-release.mjs --dir <pack out dir> --tag <tag>
//          [--repo <owner/name>] [--confirm] [--gh <path>]
import { createHash } from 'node:crypto';
import { readFile, stat } from 'node:fs/promises';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { parseArgs } from 'node:util';
import { fileURLToPath } from 'node:url';
import {
  checkRunnerContents,
  listTarGz,
  validateRunnerManifest,
} from '../packages/react-native-qnx-cli/src/runner.mjs';

export const APPROVAL_ENV = 'RNQNX_RELEASE_OWNER_APPROVED';
const DEFAULT_REPO = 'mikevocalz/react-native-qnx';
const TAG = /^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$/;
const REPO = /^[A-Za-z0-9._-]+\/[A-Za-z0-9._-]+$/;

/** @param {string} word */
const quote = (word) =>
  /^[A-Za-z0-9_./:=@%+-]+$/.test(word) ? word : `'${word.replaceAll("'", `'\\''`)}'`;

/** @param {string} file */
async function sha256Of(file) {
  return createHash('sha256')
    .update(await readFile(file))
    .digest('hex');
}

/**
 * Check a packed directory and describe the release. Reads files, writes none.
 * @param {{dir: string, tag: string, repo?: string}} options
 */
export async function planRelease(options) {
  if (!TAG.test(options.tag) || options.tag === 'latest')
    throw new Error(`--tag ${JSON.stringify(options.tag)} is not a usable release tag.`);
  const repo = options.repo ?? DEFAULT_REPO;
  if (!REPO.test(repo)) throw new Error(`--repo ${JSON.stringify(repo)} is not owner/name.`);
  const dir = path.resolve(options.dir);
  const manifestFile = path.join(dir, 'rnqnx-runner-manifest.json');
  let manifest;
  try {
    manifest = validateRunnerManifest(JSON.parse(await readFile(manifestFile, 'utf8')));
  } catch (error) {
    throw new Error(
      `${manifestFile} is not a usable runner manifest (${error instanceof Error ? error.message : error}).`
    );
  }
  if (manifest.runners.length !== 1)
    throw new Error(`Expected one runner in the manifest, found ${manifest.runners.length}.`);
  const entry = manifest.runners[0];
  const archiveFile = path.join(dir, path.basename(entry.url));
  if (path.basename(entry.url) !== entry.url)
    throw new Error(`The manifest url ${entry.url} must be a bare file name.`);
  const size = await stat(archiveFile).then(
    (info) => info.size,
    () => {
      throw new Error(`${archiveFile} is missing; run rnqnx runner pack first.`);
    }
  );
  const archiveSha = await sha256Of(archiveFile);
  if (archiveSha !== entry.sha256 || size !== entry.size)
    throw new Error(
      `${entry.url} does not match its manifest entry (sha256 ${archiveSha}, ${size} bytes; the manifest says ${entry.sha256}, ${entry.size} bytes).`
    );
  checkRunnerContents(entry, await listTarGz(archiveFile));

  const files = [archiveFile, manifestFile];
  const descriptorFile = path.join(dir, 'rnqnx-runtime.json');
  if (
    await stat(descriptorFile).then(
      () => true,
      () => false
    )
  )
    files.push(descriptorFile);
  const assets = [];
  for (const file of files)
    assets.push({ name: path.basename(file), file, sha256: await sha256Of(file) });

  const args = [
    'release',
    'create',
    options.tag,
    '--repo',
    repo,
    '--draft',
    '--title',
    `Runner ${entry.id}`,
    '--notes',
    `Prebuilt development runner ${entry.id} for ${entry.target}. Archive SHA-256 ${entry.sha256}. Draft until the Runner release attestation workflow has run.`,
    ...assets.map((asset) => asset.file),
  ];
  return {
    tag: options.tag,
    repo,
    runnerId: entry.id,
    target: entry.target,
    assets,
    args,
    command: ['gh', ...args].map(quote).join(' '),
  };
}

/**
 * @param {string[]} argv
 * @param {{env?: NodeJS.ProcessEnv, out?: (line: string) => void, err?: (line: string) => void}} [io]
 * @returns {Promise<number>} exit code
 */
export async function publishMain(argv, io = {}) {
  const env = io.env ?? process.env;
  const out = io.out ?? ((line) => console.log(line));
  const err = io.err ?? ((line) => console.error(line));
  try {
    const { values } = parseArgs({
      args: argv,
      strict: true,
      options: {
        dir: { type: 'string' },
        tag: { type: 'string' },
        repo: { type: 'string' },
        gh: { type: 'string' },
        confirm: { type: 'boolean' },
        help: { type: 'boolean', short: 'h' },
      },
    });
    if (values.help || !values.dir || !values.tag) {
      out(
        'Usage: node scripts/publish-runner-release.mjs --dir <pack out dir> --tag <tag> [--repo <owner/name>] [--confirm]\nDry run by default. --confirm and RNQNX_RELEASE_OWNER_APPROVED=<tag> are both required to create the draft release.'
      );
      return values.help ? 0 : 2;
    }
    const plan = await planRelease({ dir: values.dir, tag: values.tag, repo: values.repo });
    out(`Release ${plan.tag} on ${plan.repo}: runner ${plan.runnerId} (${plan.target})`);
    for (const asset of plan.assets) out(`  ${asset.sha256}  ${asset.name}`);
    out('SHA256SUMS is added by the Runner release attestation workflow.');
    out(`Command: ${plan.command}`);
    if (!values.confirm) {
      out('Dry run: nothing was created. Pass --confirm to run the command.');
      return 0;
    }
    if (env[APPROVAL_ENV] !== plan.tag) {
      err(
        `Refusing to create the release: ${APPROVAL_ENV} must be set to ${plan.tag}, and it is ${env[APPROVAL_ENV] === undefined ? 'not set' : 'set to a different value'}. The owner sets it.`
      );
      return 1;
    }
    const result = spawnSync(values.gh ?? 'gh', plan.args, { stdio: 'inherit', shell: false });
    if (result.error) throw result.error;
    if (result.status !== 0) {
      err(`gh exited ${result.status}.`);
      return 1;
    }
    out(
      'Draft release created. Next: run the Runner release attestation workflow, then publish the draft (ADR-0018 steps 4 and 5).'
    );
    return 0;
  } catch (error) {
    err(error instanceof Error ? error.message : String(error));
    return 1;
  }
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url))
  process.exitCode = await publishMain(process.argv.slice(2));
