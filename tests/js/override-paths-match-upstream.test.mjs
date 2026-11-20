import assert from 'node:assert/strict';
import { existsSync, readdirSync, readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import path from 'node:path';

// The Metro resolver in packages/metro-config-qnx swaps a React Native file for
// the platform package's copy only when the copy sits at the same relative path
// under `Libraries/` (or, for a module React Native ships only as `.ios.js` and
// `.android.js`, under `src/private/`). A file at any other path is never
// selected: the upstream component loads and the override is dead code. This
// test fails when an override path does not match a file in the pinned
// react-native, and when the resolver does not actually pick it.
const require = createRequire(import.meta.url);
const root = process.cwd();
const platformRoot = path.join(root, 'packages/react-native-qnx');

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

/** Files that are not path overrides. Each is loaded by an explicit path. */
const EXPLICIT_ENTRIES = new Map([
  [
    'Libraries/ReactNative/PublicAPI.js',
    {
      // metro-config-qnx/public-entry.js maps the `react-native` specifier here.
      referencedBy: 'packages/metro-config-qnx/public-entry.js',
      needle: "'ReactNative/PublicAPI.js'",
    },
  ],
]);

/** @param {string} dir @returns {string[]} paths relative to dir */
function listJs(dir) {
  /** @type {string[]} */
  const out = [];
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

const { createQnxResolveRequest } = require('../../packages/metro-config-qnx/src/index.js');
const overridesRoot = path.join(platformRoot, 'Libraries');
const resolveRequest = createQnxResolveRequest(overridesRoot);

/**
 * Runs the real resolver as Metro would, with Metro's own result stubbed.
 * @param {string} moduleName
 * @param {string | null} upstreamFile File Metro would resolve, or null when it cannot.
 * @param {string} origin Importing module.
 * @returns {{ type: string, filePath: string }}
 */
function resolveThroughQnx(moduleName, upstreamFile, origin) {
  const context = {
    originModulePath: origin,
    resolveRequest() {
      if (!upstreamFile) throw new Error(`Unable to resolve ${moduleName}`);
      return { type: 'sourceFile', filePath: upstreamFile };
    },
  };
  return /** @type {{ type: string, filePath: string }} */ (
    resolveRequest(context, moduleName, 'qnx')
  );
}

const checked = [];
for (const relative of listJs(overridesRoot)) {
  const key = `Libraries/${relative}`;
  const explicit = EXPLICIT_ENTRIES.get(key);
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
  const result = resolveThroughQnx('./probe', upstream, upstream);
  assert.equal(
    result.filePath,
    path.join(overridesRoot, relative),
    `${key}: the resolver did not select the override`
  );
  checked.push(key);
}

// src/private overrides are reachable only through the resolver's fallback,
// which runs when React Native has `X.ios.js` and `X.android.js` but no `X.js`.
// If upstream has `X.js`, Metro resolves it and the override is never read.
const privateRoot = path.join(platformRoot, 'src/private');
for (const relative of listJs(privateRoot)) {
  const key = `src/private/${relative}`;
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
  const result = resolveThroughQnx(`./${path.basename(stem)}`, null, importer);
  assert.equal(
    result.filePath,
    path.join(privateRoot, relative),
    `${key}: the resolver fallback did not select the override`
  );
  checked.push(key);
}

assert.ok(checked.length > 0, 'no overrides were checked');
console.log(`override paths: ${checked.length} overrides match react-native@${installed}`);
