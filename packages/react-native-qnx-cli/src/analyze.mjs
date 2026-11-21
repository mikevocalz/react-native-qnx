import { readFile, realpath, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { parseArgs } from 'node:util';
import { discoverExpoModules, discoverNitroModules, locatePackage } from './nitro-modules.mjs';
import {
  APP_SCHEMA,
  bundleProfile,
  compareRunnerToBundle,
  readRuntimeDescriptor,
} from './runtime-compat.mjs';

// `rnqnx analyze`: what native code does this app bring, and can the stock
// prebuilt runner load it? Reads JSON only (package.json files); no package
// code runs. The answer is one of three: the stock runner is enough, it is
// not (a custom development build is needed), or it cannot be decided because
// no runner description was given and the app has native modules.

const help = `React Native QNX app analysis
  analyze --project <directory> [--platform qnx|marathon] [--runtime <rnqnx-runtime.json>]
          [--json]

Lists the app's direct dependencies that add native code for the platform:
  Nitro modules      package.json "react-native-qnx": {"nitro": ...} (ADR-0019)
  Expo modules       package.json "react-native-qnx": {"expo": ...}
  Fabric components  package.json "codegenConfig" with type "components" or "all"
and says whether the stock prebuilt runner is sufficient, with the reason.
--runtime names the rnqnx-runtime.json from the runner you plan to use (it sits
at the top of the runner archive). Without it, an app with no native modules is
sufficient on modules, and an app with native modules gets "undetermined".
With --runtime, the React Native version and Nitro ABI are compared too; the
Hermes bytecode version needs the compiled bundle and is checked at launch.
Module checks compare package name and exact version. Version agreement of
React Native, Hermes bytecode and Nitro is checked at launch: project run
--runtime.
`;

/** @typedef {import('./runtime-compat.mjs').PackageRef} PackageRef */
/** @typedef {{nitroModules: PackageRef[], expoModules: PackageRef[], fabricComponents: PackageRef[]}} AppNative */

/**
 * Direct dependencies that declare Fabric components through codegenConfig.
 * `react-native` itself is the runner's own base and is skipped.
 * @param {string} root @returns {Promise<PackageRef[]>}
 */
export async function discoverFabricComponents(root) {
  const manifest = JSON.parse(await readFile(path.join(root, 'package.json'), 'utf8'));
  const dependencies = manifest.dependencies ?? {};
  if (!dependencies || typeof dependencies !== 'object' || Array.isArray(dependencies))
    throw new Error('Project package.json "dependencies" must be an object.');
  /** @type {PackageRef[]} */
  const found = [];
  for (const name of Object.keys(dependencies).sort()) {
    if (name === 'react-native') continue;
    const packageRoot = await locatePackage(root, name);
    if (!packageRoot) continue;
    const metadata = JSON.parse(await readFile(path.join(packageRoot, 'package.json'), 'utf8'));
    const type = metadata.codegenConfig?.type;
    if (type !== 'components' && type !== 'all') continue;
    if (typeof metadata.version !== 'string' || !metadata.version)
      throw new Error(`${name}: package.json must declare a version.`);
    found.push({ package: name, version: metadata.version });
  }
  return found;
}

/**
 * @param {string} root realpath of the project @param {string} platform
 * @returns {Promise<AppNative>}
 */
export async function appNative(root, platform) {
  const toRef = (/** @type {{package: string, version: string}} */ m) => ({
    package: m.package,
    version: m.version,
  });
  return {
    nitroModules: (await discoverNitroModules(root, platform)).map(toRef),
    expoModules: (await discoverExpoModules(root, platform)).map(toRef),
    fabricComponents: await discoverFabricComponents(root),
  };
}

/**
 * What the native runner reads before it runs the bundle (rnqnx-app.json,
 * docs/nitro/API_CONTRACTS.md "S6 native runner preflight"). Same discovery
 * and the same exact versions as `analyze`, so the host verdict and the
 * native screen cannot disagree about which modules the app needs.
 * @param {string} root realpath of the project @param {string} platform
 */
export async function appRequirements(root, platform) {
  const profile = await bundleProfile(root, undefined);
  return {
    schema: APP_SCHEMA,
    reactNative: profile.reactNative,
    nitroAbi: profile.nitroAbi,
    requires: await appNative(root, platform),
  };
}

/**
 * Write rnqnx-app.json. Deploy puts it next to the executable.
 * @param {string} root realpath of the project @param {string} platform @param {string} file
 */
export async function writeAppRequirements(root, platform, file) {
  const requirements = await appRequirements(root, platform);
  await writeFile(file, `${JSON.stringify(requirements, null, 2)}\n`);
  return requirements;
}

/**
 * Decide whether a runner that links `builtins` can host the app's native
 * modules. A module is covered only by the same package at the same version:
 * another version is different native code.
 * @param {AppNative} app
 * @param {import('./runtime-compat.mjs').Builtins | null} builtins
 * @returns {{sufficient: boolean | null, reason: string, uncovered: {kind: string, package: string, version: string, runner: string | null}[]}}
 */
export function stockRunnerVerdict(app, builtins) {
  const kinds = /** @type {const} */ ([
    ['Nitro module', 'nitroModules'],
    ['Expo module', 'expoModules'],
    ['Fabric component', 'fabricComponents'],
  ]);
  const total = kinds.reduce((sum, [, key]) => sum + app[key].length, 0);
  if (total === 0)
    return {
      sufficient: true,
      reason:
        'The app adds no Nitro modules, Expo modules or Fabric components, so the stock runner has all the native code it needs. Versions are checked at launch.',
      uncovered: [],
    };
  if (!builtins)
    return {
      sufficient: null,
      reason: `The app adds ${total} native ${total === 1 ? 'module' : 'modules'} and no runner description was given. Pass --runtime <rnqnx-runtime.json> from the runner you plan to use.`,
      uncovered: [],
    };
  /** @type {{kind: string, package: string, version: string, runner: string | null}[]} */
  const uncovered = [];
  for (const [kind, key] of kinds)
    for (const item of app[key]) {
      const have = builtins[key].find((b) => b.package === item.package);
      if (!have || have.version !== item.version)
        uncovered.push({
          kind,
          package: item.package,
          version: item.version,
          runner: have ? have.version : null,
        });
    }
  if (!uncovered.length)
    return {
      sufficient: true,
      reason: `The runner links every one of the app's ${total} native ${total === 1 ? 'module' : 'modules'} at the same version.`,
      uncovered,
    };
  const first = uncovered
    .slice(0, 3)
    .map(
      (u) => `${u.package} ${u.version} (${u.runner ? `runner has ${u.runner}` : 'not in runner'})`
    )
    .join(', ');
  return {
    sufficient: false,
    reason: `The runner does not link ${uncovered.length} of the app's native ${total === 1 ? 'module' : 'modules'}: ${first}${uncovered.length > 3 ? ', ...' : ''}. Build a custom development build: rnqnx project build-native --help.`,
    uncovered,
  };
}

/**
 * @param {{project: string, platform?: string, runtime?: string}} options
 */
export async function analyzeProject(options) {
  const platform = options.platform ?? 'qnx';
  if (!['qnx', 'marathon'].includes(platform)) throw new Error('Use --platform qnx or marathon.');
  const root = await realpath(path.resolve(options.project));
  const app = await appNative(root, platform);
  const runtime = options.runtime ? await readRuntimeDescriptor(options.runtime) : null;
  let verdict = stockRunnerVerdict(app, runtime ? runtime.builtins : null);
  if (runtime) {
    // Modules can all be linked and the runner can still be the wrong build.
    const { mismatches } = compareRunnerToBundle(runtime, await bundleProfile(root, undefined));
    if (mismatches.length)
      verdict = {
        sufficient: false,
        reason: `The runner and the app disagree on ${mismatches
          .map((m) => `${m.label} (runner ${m.runner}, app ${m.bundle})`)
          .join(
            ', '
          )}. Use a runner built for this app, or build a custom development build: rnqnx project build-native --help.`,
        uncovered: verdict.uncovered,
      };
  }
  return {
    project: root,
    platform,
    runner: runtime ? runtime.runnerId : null,
    nitroModules: app.nitroModules,
    expoModules: app.expoModules,
    fabricComponents: app.fabricComponents,
    stockRunner: verdict,
  };
}

/** @param {string[]} argv */
export async function analyzeMain(argv) {
  const { values, positionals } = parseArgs({
    args: argv,
    strict: true,
    allowPositionals: true,
    options: {
      help: { type: 'boolean', short: 'h' },
      project: { type: 'string' },
      platform: { type: 'string' },
      runtime: { type: 'string' },
      json: { type: 'boolean' },
    },
  });
  if (values.help) {
    console.log(help);
    return 0;
  }
  if (positionals.length) throw new Error('analyze accepts no positional arguments.');
  const report = await analyzeProject({
    project: values.project ?? '.',
    platform: values.platform,
    runtime: values.runtime,
  });
  if (values.json) {
    console.log(JSON.stringify(report, null, 2));
    return 0;
  }
  /** @param {string} title @param {PackageRef[]} items */
  const list = (title, items) => {
    console.log(`${title}: ${items.length ? '' : 'none'}`);
    for (const item of items) console.log(`  ${item.package} ${item.version}`);
  };
  list('Nitro modules', report.nitroModules);
  list('Expo modules', report.expoModules);
  list('Fabric components', report.fabricComponents);
  const { sufficient, reason } = report.stockRunner;
  console.log(
    `Stock runner: ${sufficient === null ? 'undetermined' : sufficient ? 'sufficient' : 'not sufficient'}`
  );
  console.log(reason);
  return 0;
}
