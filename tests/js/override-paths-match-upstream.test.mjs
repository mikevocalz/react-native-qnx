import assert from 'node:assert/strict';
import { existsSync, readdirSync, readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import path from 'node:path';

// The Metro resolvers in packages/metro-config-qnx and
// packages/metro-config-marathon swap a React Native file for a platform copy
// only when the copy sits at the same relative path under `Libraries/` (or,
// for a module React Native ships only as `.ios.js` and `.android.js`, under
// `src/private/`). A file at any other path is never selected: the upstream
// component loads and the override is dead code. This test fails when an
// override path does not match a file in the pinned react-native, and when the
// platform's real resolver does not pick it.
const require = createRequire(import.meta.url);
const root = process.cwd();

const pinned = JSON.parse(readFileSync(path.join(root, 'package.json'), 'utf8')).devDependencies[
  'react-native'
];
const rnRoot = path.dirname(require.resolve('react-native/package.json'));
const installed = JSON.parse(readFileSync(path.join(rnRoot, 'package.json'), 'utf8')).version;
assert.equal(
  installed,
  pinned,
  `installed react-native ${installed} differs from the pin ${pinned}`
);

const { createQnxResolveRequest } = require('../../packages/metro-config-qnx/src/index.js');
const {
  createMarathonResolveRequest,
} = require('../../packages/metro-config-marathon/src/index.js');

/**
 * @typedef {{
 *   name: string,
 *   platform: string,
 *   packageRoot: string,
 *   resolveRequest: (context: any, moduleName: string, platform: string | null) => any,
 *   explicit: Map<string, { referencedBy: string, needle: string }>,
 * }} OverridePackage
 */

/** @type {OverridePackage[]} */
const packages = [
  {
    name: 'react-native-qnx',
    platform: 'qnx',
    packageRoot: path.join(root, 'packages/react-native-qnx'),
    resolveRequest: createQnxResolveRequest(path.join(root, 'packages/react-native-qnx/Libraries')),
    // Files that are not path overrides; each is loaded by an explicit path.
    explicit: new Map([
      [
        'Libraries/ReactNative/PublicAPI.js',
        {
          // metro-config-qnx/public-entry.js maps `react-native` to this file.
          referencedBy: 'packages/metro-config-qnx/public-entry.js',
          needle: "'ReactNative/PublicAPI.js'",
        },
      ],
    ]),
  },
  {
    name: 'react-native-marathon',
    platform: 'marathon',
    packageRoot: path.join(root, 'packages/react-native-marathon'),
    resolveRequest: createMarathonResolveRequest(
      path.join(root, 'packages/react-native-marathon/Libraries'),
      path.join(root, 'packages/react-native-qnx/Libraries')
    ),
    explicit: new Map(),
  },
];

/** @param {string} dir @returns {string[]} paths relative to dir, empty when dir is absent */
function listJs(dir) {
  /** @type {string[]} */
  const out = [];
  if (!existsSync(dir)) return out;
  /** @param {string} current */
  const walk = (current) => {
    for (const entry of readdirSync(current, { withFileTypes: true })) {
      const full = path.join(current, entry.name);
      if (entry.isDirectory()) walk(full);
      else if (entry.name.endsWith('.js')) out.push(path.relative(dir, full));
    }
  };
  walk(dir);
  return out.sort();
}

/**
 * Runs a platform's real resolver as Metro would, with Metro's own result
 * stubbed.
 * @param {OverridePackage} pkg
 * @param {string} moduleName
 * @param {string | null} upstreamFile File Metro would resolve, or null when it cannot.
 * @param {string} origin Importing module.
 * @returns {{ type: string, filePath: string }}
 */
function resolveThrough(pkg, moduleName, upstreamFile, origin) {
  const context = {
    originModulePath: origin,
    resolveRequest() {
      if (!upstreamFile) throw new Error(`Unable to resolve ${moduleName}`);
      return { type: 'sourceFile', filePath: upstreamFile };
    },
  };
  return /** @type {{ type: string, filePath: string }} */ (
    pkg.resolveRequest(context, moduleName, pkg.platform)
  );
}

/** @type {string[]} */
const checked = [];
for (const pkg of packages) {
  const overridesRoot = path.join(pkg.packageRoot, 'Libraries');
  for (const relative of listJs(overridesRoot)) {
    const key = `${pkg.name}/Libraries/${relative}`;
    const explicit = pkg.explicit.get(`Libraries/${relative}`);
    if (explicit) {
      const referrer = readFileSync(path.join(root, explicit.referencedBy), 'utf8');
      assert.ok(
        referrer.includes(explicit.needle),
        `${key} is not referenced by ${explicit.referencedBy}`
      );
      continue;
    }
    const upstream = path.join(rnRoot, 'Libraries', relative);
    assert.ok(
      existsSync(upstream),
      `${key} has no counterpart at react-native@${installed} Libraries/${relative}. ` +
        'The resolver matches overrides by relative path, so this file is never selected. ' +
        'Move it to the upstream path.'
    );
    const result = resolveThrough(pkg, './probe', upstream, upstream);
    assert.equal(
      result.filePath,
      path.join(overridesRoot, relative),
      `${key}: the ${pkg.platform} resolver did not select the override`
    );
    checked.push(key);
  }

  // src/private overrides are reachable only through the resolver's fallback,
  // which runs when React Native has `X.ios.js` and `X.android.js` but no
  // `X.js`. If upstream has `X.js`, Metro resolves it and the override is
  // never read.
  const privateRoot = path.join(pkg.packageRoot, 'src/private');
  for (const relative of listJs(privateRoot)) {
    const key = `${pkg.name}/src/private/${relative}`;
    const stem = relative.slice(0, -'.js'.length);
    const upstreamBase = path.join(rnRoot, 'src/private', stem);
    assert.ok(
      !existsSync(`${upstreamBase}.js`),
      `${key}: react-native has src/private/${stem}.js, so Metro resolves it and the override is never read`
    );
    assert.ok(
      existsSync(`${upstreamBase}.ios.js`) || existsSync(`${upstreamBase}.android.js`),
      `${key} has no platform-suffixed counterpart in react-native@${installed} src/private`
    );
    const importer = path.join(rnRoot, 'src/private', path.dirname(relative), 'importer.js');
    const result = resolveThrough(pkg, `./${path.basename(stem)}`, null, importer);
    assert.equal(
      result.filePath,
      path.join(privateRoot, relative),
      `${key}: the ${pkg.platform} resolver fallback did not select the override`
    );
    checked.push(key);
  }
}

assert.ok(checked.length > 0, 'no overrides were checked');
console.log(`override paths: ${checked.length} overrides match react-native@${installed}`);
