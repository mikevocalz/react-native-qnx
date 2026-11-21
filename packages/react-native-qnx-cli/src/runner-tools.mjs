import { readFile, realpath } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { parseArgs } from 'node:util';
import { checkoutRunnerVersions } from './runner.mjs';
import { packRunner } from './runner-pack.mjs';
import {
  bundleProfile,
  compareRunnerToBundle,
  formatMismatch,
  readRuntimeDescriptor,
} from './runtime-compat.mjs';

/**
 * The launch-time compatibility check. Throws the readable error naming both
 * sides when the runner and the bundle disagree.
 * @param {{runtime: string, project: string, bundle?: string}} options
 */
export async function launchCheck(options) {
  const runtime = await readRuntimeDescriptor(options.runtime);
  const root = await realpath(path.resolve(options.project));
  const profile = await bundleProfile(
    root,
    options.bundle ? path.resolve(options.bundle) : undefined
  );
  const result = compareRunnerToBundle(runtime, profile);
  if (!result.ok) throw new Error(formatMismatch(runtime.runnerId, result.mismatches));
  if (result.compared === 0)
    throw new Error(
      `Nothing could be compared with runner ${runtime.runnerId}.\n${result.notChecked.join('\n')}\nInstall the app's dependencies and pass --bundle with a compiled .hbc file.`
    );
  return { runner: runtime.runnerId, ...result };
}

/** Exact pinned Nitro runtime of this checkout (package.json "nitro-core"). */
async function checkoutNitroAbi() {
  const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
  const workspace = JSON.parse(await readFile(path.join(root, 'package.json'), 'utf8'));
  const pin = workspace.devDependencies?.['nitro-core'] ?? workspace.dependencies?.['nitro-core'];
  if (typeof pin !== 'string' || !/^\d/.test(pin))
    throw new Error(
      `This checkout's nitro-core entry (${JSON.stringify(pin)}) is not an exact version. Pass --nitro-abi <version>.`
    );
  return pin;
}

/** @param {string[]} argv */
export async function runnerToolsMain(argv) {
  const [command, ...rest] = argv;
  const { values, positionals } = parseArgs({
    args: rest,
    strict: true,
    allowPositionals: true,
    options: {
      help: { type: 'boolean', short: 'h' },
      runtime: { type: 'string' },
      project: { type: 'string' },
      bundle: { type: 'string' },
      stage: { type: 'string' },
      out: { type: 'string' },
      id: { type: 'string' },
      target: { type: 'string' },
      hermesc: { type: 'string' },
      hermes: { type: 'string' },
      qt: { type: 'string' },
      sdp: { type: 'string' },
      builtins: { type: 'string' },
      'nitro-abi': { type: 'string' },
      'react-native-qnx': { type: 'string' },
      'react-native': { type: 'string' },
      'target-libs': { type: 'string' },
    },
  });
  if (positionals.length) throw new Error(`runner ${command} accepts no positional arguments.`);
  if (command === 'check') {
    if (!values.runtime) throw new Error('runner check needs --runtime <rnqnx-runtime.json>.');
    const result = await launchCheck({
      runtime: values.runtime,
      project: values.project ?? '.',
      bundle: values.bundle,
    });
    console.log(JSON.stringify(result, null, 2));
    return 0;
  }
  for (const key of /** @type {const} */ ([
    'stage',
    'out',
    'id',
    'target',
    'hermesc',
    'hermes',
    'qt',
    'sdp',
    'builtins',
  ]))
    if (!values[key]) throw new Error(`runner pack needs --${key}.`);
  const pinned = await checkoutRunnerVersions();
  const builtins = JSON.parse(await readFile(/** @type {string} */ (values.builtins), 'utf8'));
  const targetLibraries = values['target-libs']
    ? (await readFile(values['target-libs'], 'utf8')).split(/\r?\n/).filter(Boolean)
    : undefined;
  const result = await packRunner({
    stage: /** @type {string} */ (values.stage),
    out: /** @type {string} */ (values.out),
    id: /** @type {string} */ (values.id),
    target: /** @type {string} */ (values.target),
    hermesc: /** @type {string} */ (values.hermesc),
    versions: {
      reactNativeQnx: values['react-native-qnx'] ?? pinned.reactNativeQnx,
      reactNative: values['react-native'] ?? pinned.reactNative,
      hermes: /** @type {string} */ (values.hermes),
      qt: /** @type {string} */ (values.qt),
      sdp: values.sdp === 'none' ? null : /** @type {string} */ (values.sdp),
    },
    nitroAbi: values['nitro-abi'] ?? (await checkoutNitroAbi()),
    builtins,
    targetLibraries,
  });
  console.log(JSON.stringify({ ...result, published: false }, null, 2));
  return 0;
}
