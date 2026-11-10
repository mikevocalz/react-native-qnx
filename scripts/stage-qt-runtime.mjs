#!/usr/bin/env node
import { constants } from 'node:fs';
import {
  mkdir,
  open,
  realpath,
  lstat,
  chmod,
  symlink,
  utimes,
  lutimes,
  rm,
} from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
import { validateCandidate, canonicalJson, LIMITS } from './lib/qt-runtime-manifest.mjs';

const EPOCH = new Date(0);
const QT_CONF = '[Paths]\nPrefix=..\nLibraries=lib\nPlugins=plugins\nQmlImports=qml\n';
const RUNTIME_PATHS = {
  relativeTo: 'candidate-root',
  environment: { LD_LIBRARY_PATH: ['lib'], QT_PLUGIN_PATH: ['plugins'], QML_IMPORT_PATH: ['qml'] },
  executable: 'bin/app',
  bundle: 'app.hbc',
  execution: 'NOT RUN',
  releaseAuthorized: false,
};

async function copyVerified(/** @type {any} */ input, /** @type {any} */ destination) {
  const source = await open(input.source, constants.O_RDONLY | constants.O_NOFOLLOW);
  let output;
  try {
    const stat = await source.stat();
    if (!stat.isFile() || stat.size !== input.size) throw new Error(`input changed: ${input.path}`);
    output = await open(destination, 'wx', input.kind === 'app' ? 0o755 : 0o644);
    const digest = createHash('sha256'),
      buffer = Buffer.alloc(1024 * 1024);
    let size = 0;
    while (true) {
      const { bytesRead } = await source.read(buffer, 0, buffer.length, null);
      if (!bytesRead) break;
      size += bytesRead;
      if (size > input.size || size > LIMITS.fileBytes)
        throw new Error(`input grew: ${input.path}`);
      digest.update(buffer.subarray(0, bytesRead));
      let position = 0;
      while (position < bytesRead) {
        const result = await output.write(buffer, position, bytesRead - position, null);
        if (!result.bytesWritten) throw new Error(`short output write: ${input.path}`);
        position += result.bytesWritten;
      }
    }
    if (size !== input.size || digest.digest('hex') !== input.sha256)
      throw new Error(`input changed: ${input.path}`);
  } finally {
    await source.close();
    if (output) await output.close();
  }
}

/** Validate local inputs and create a new candidate directory. Never executes staged content. */
export async function stageQtRuntime(/** @type {any} */ options) {
  if (typeof process.getuid === 'function' && process.getuid() === 0)
    throw new Error('refusing root execution');
  const candidate = await validateCandidate(options);
  const requested = path.resolve(options.output);
  const outputParent = await realpath(path.dirname(requested));
  const output = path.join(outputParent, path.basename(requested));
  for (const root of [candidate.prefix, candidate.materialRoot]) {
    const relative = path.relative(root, output);
    if (
      relative === '' ||
      (!relative.startsWith(`..${path.sep}`) && relative !== '..' && !path.isAbsolute(relative))
    ) {
      throw new Error('output must be outside the Qt prefix and manifest/material root');
    }
  }
  if ((await lstat(outputParent)).isDirectory() === false)
    throw new Error('output parent must be a directory');
  // Exclusive mkdir rejects existing files, directories and dangling symlinks without modifying them.
  await mkdir(output, { mode: 0o755 });
  const directories = new Set([output]);
  const inventory = [];
  const ensureParent = async (/** @type {any} */ relative) => {
    const directory = path.dirname(path.join(output, relative));
    await mkdir(directory, { recursive: true, mode: 0o755 });
    for (let current = directory; current !== output; current = path.dirname(current))
      directories.add(current);
    return path.join(output, relative);
  };
  const generated = async (/** @type {any} */ relative, /** @type {any} */ content) => {
    const destination = await ensureParent(relative);
    const handle = await open(destination, 'wx', 0o644);
    try {
      await handle.writeFile(content);
    } finally {
      await handle.close();
    }
    await chmod(destination, 0o644);
    await utimes(destination, EPOCH, EPOCH);
    inventory.push({
      path: relative,
      kind: 'generated',
      sha256: createHash('sha256').update(content).digest('hex'),
      size: Buffer.byteLength(content),
      mode: '0644',
    });
  };
  try {
    for (const input of candidate.inputs) {
      const destination = await ensureParent(input.path);
      if (input.kind === 'symlink') {
        await symlink(input.target, destination);
        await lutimes(destination, EPOCH, EPOCH);
      } else {
        await copyVerified(input, destination);
        await chmod(destination, input.kind === 'app' ? 0o755 : 0o644);
        await utimes(destination, EPOCH, EPOCH);
      }
      const { source, ...record } = /** @type {any} */ (input);
      inventory.push({
        ...record,
        mode: input.kind === 'symlink' ? 'symlink' : input.kind === 'app' ? '0755' : '0644',
      });
    }
    await generated('candidate-input.json', canonicalJson(candidate.manifest));
    await generated('bin/qt.conf', QT_CONF);
    await generated('runtime-paths.json', canonicalJson(RUNTIME_PATHS));
    const receipt = {
      schemaVersion: 1,
      kind: 'offline-qt-runtime-candidate',
      execution: 'NOT RUN',
      releaseAuthorized: false,
      profile: candidate.manifest.profile,
      manifestSha256: candidate.manifestSha256,
      repository: candidate.manifest.repository,
      app: candidate.manifest.app,
      qt: candidate.manifest.qt,
      abi: candidate.manifest.abi,
      qmlClosure: candidate.qmlClosure,
      dependencies: candidate.dependencies.sort((a, b) =>
        `${a.file}/${a.needed}`.localeCompare(`${b.file}/${b.needed}`, 'en')
      ),
      systemDependencies: candidate.manifest.systemDependencies,
      materials: candidate.manifest.materials,
      components: candidate.manifest.components,
      replacement: candidate.manifest.replacement,
      files: [...inventory].sort((a, b) => a.path.localeCompare(b.path, 'en')),
      limitations: [
        'ABI header checks do not distinguish every QNX/Linux binary or prove toolchain provenance.',
        'Build/source/system receipts are supplied assertions with verified local material hashes, not independent certification.',
        'Dynamic imports/plugins must be enumerated; source scans cannot prove arbitrary computed loading.',
        'No execution, replacement demonstration, legal certification or release gate promotion.',
      ],
    };
    await generated('candidate-receipt.json', canonicalJson(receipt));
    for (const directory of [...directories].sort((a, b) => b.length - a.length)) {
      await chmod(directory, 0o755);
      await utimes(directory, EPOCH, EPOCH);
    }
    return receipt;
  } catch (error) {
    // Only this invocation's exclusively-created directory is removed.
    await rm(output, { recursive: true, force: true });
    throw error;
  }
}

export async function main(args = process.argv.slice(2)) {
  const keys = new Map([
    ['--manifest', 'manifest'],
    ['--qt-prefix', 'qtPrefix'],
    ['--app', 'app'],
    ['--bundle', 'bundle'],
    ['--output', 'output'],
  ]);
  if (args.length !== 10)
    throw new Error(
      'usage: stage-qt-runtime.mjs --manifest candidate.json --qt-prefix PREFIX --app ELF --bundle matched.hbc --output NEW_DIRECTORY'
    );
  /** @type {Record<string, string>} */
  const options = {};
  for (let i = 0; i < args.length; i += 2) {
    const key = keys.get(args[i]),
      value = args[i + 1];
    if (
      !key ||
      Object.hasOwn(options, key) ||
      !value ||
      value.startsWith('--') ||
      value.includes('\0')
    )
      throw new Error('invalid/duplicate staging option');
    options[key] = value;
  }
  await stageQtRuntime(options);
  process.stdout.write(
    'Offline Qt runtime candidate staged; execution NOT RUN; releaseAuthorized=false\n'
  );
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  main().catch((error) => {
    process.stderr.write(`${error.message}\n`);
    process.exitCode = 1;
  });
}
