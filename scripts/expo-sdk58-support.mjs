#!/usr/bin/env node
// Expo SDK 58 package support list for the QNX target.
//
//   node scripts/expo-sdk58-support.mjs --write   regenerate (network: jsDelivr)
//   node scripts/expo-sdk58-support.mjs --check   offline drift check
//
// --write takes every package in the installed expo's bundledNativeModules.json,
// resolves its SDK range to a version, lists that version's published files and
// reads expo-module.config.json when present, then classifies it with
// classifyPackage(). --check fails when the installed expo or its package/range
// list no longer matches the committed file, or when the committed rows no longer
// follow classifyPackage() for their recorded evidence.
//
// The statuses describe package contents only. None of them is a device run.

import { readFile, writeFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
export const SUPPORT_FILE = path.join(root, 'docs/expo-delivery/sdk58-support.json');
const SCHEMA = 'rnqnx-expo-sdk-support/v1';

// Packages whose role cannot be read from their files. Each has a reason.
export const OVERRIDES = Object.freeze({
  react: ['js-only', 'Renderer-independent React; QNX uses it through react-native.'],
  'react-native': ['platform', 'Provided on QNX by @rn-qnx/react-native-qnx out of tree.'],
  'react-dom': ['no-op', 'Web renderer; not part of a QNX bundle.'],
  'react-native-web': ['no-op', 'Web target only; not part of a QNX bundle.'],
  'react-server-dom-webpack': ['no-op', 'React Server Components for web/server output.'],
  'eslint-config-expo': ['host-tooling', 'Lint config; runs on the development host.'],
  'jest-expo': ['host-tooling', 'Test preset; runs on the development host.'],
  '@expo/fingerprint': ['host-tooling', 'Build fingerprinting on the development host.'],
  'expo-module-template': ['host-tooling', 'Template for create-expo-module.'],
  'expo-modules-core': [
    'partial',
    'Upstream native core is iOS/Android; packages/expo-modules-core-qnx supplies the QNX JSI host contract, covered by host tests only.',
  ],
  'expo-server': [
    'host-tooling',
    'Server API for Expo Router API routes; runs on the server or development host, not in a QNX app bundle.',
  ],
  'expo-mcp': ['host-tooling', 'Local MCP capabilities provider for Expo; a development-host CLI.'],
  'expo-build-properties': ['no-op', 'Config plugin for iOS/Android build properties only.'],
});

/**
 * @param {string} name
 * @param {{files: string[], moduleConfig: any}} evidence
 * @returns {{status: string, reason: string}}
 */
export function classifyPackage(name, evidence) {
  const override = /** @type {Record<string, string[]>} */ (OVERRIDES)[name];
  if (override) return { status: override[0], reason: override[1] };
  const platforms = Array.isArray(evidence.moduleConfig?.platforms)
    ? evidence.moduleConfig.platforms
    : [];
  if (platforms.includes('qnx'))
    return { status: 'qnx-native', reason: 'expo-module.config.json declares qnx.' };
  const native = evidence.files.some((file) =>
    /^\/(ios|android|apple|cpp|common\/cpp)\/|\.podspec$|^\/android\/build\.gradle/.test(file)
  );
  if (
    native ||
    platforms.some((/** @type {string} */ p) => ['ios', 'apple', 'android'].includes(p))
  )
    return {
      status: 'unsupported',
      reason: `Native code for ${platforms.length ? platforms.join(', ') : 'ios/android'} only; no qnx implementation.`,
    };
  if (platforms.includes('web'))
    return { status: 'no-op', reason: 'Declares only the web platform.' };
  return { status: 'js-only', reason: 'No native code published; bundles as JavaScript.' };
}

async function installedSdk() {
  const require = createRequire(path.join(root, 'package.json'));
  const expoManifest = require.resolve('expo/package.json');
  const expo = JSON.parse(await readFile(expoManifest, 'utf8'));
  const bundled = JSON.parse(
    await readFile(path.join(path.dirname(expoManifest), 'bundledNativeModules.json'), 'utf8')
  );
  return { expoVersion: expo.version, bundled: /** @type {Record<string, string>} */ (bundled) };
}

/** @param {string} url @returns {Promise<any>} */
async function json(url) {
  for (let attempt = 1; ; attempt++) {
    const response = await fetch(url);
    if (response.ok) return response.json();
    if (attempt === 3 || response.status === 404) throw new Error(`${response.status} for ${url}`);
    await new Promise((resolve) => setTimeout(resolve, 1000 * attempt));
  }
}

async function generate() {
  const { expoVersion, bundled } = await installedSdk();
  const api = 'https://data.jsdelivr.com/v1/packages/npm';
  const rows = [];
  for (const [name, range] of Object.entries(bundled).sort(([a], [b]) => a.localeCompare(b))) {
    const resolved = await json(`${api}/${name}/resolved?specifier=${encodeURIComponent(range)}`);
    if (!resolved.version) throw new Error(`No published version of ${name} matches ${range}.`);
    const listing = await json(`${api}/${name}@${resolved.version}?structure=flat`);
    const files = listing.files.map((/** @type {{name: string}} */ f) => f.name);
    let moduleConfig = null;
    if (files.includes('/expo-module.config.json')) {
      const response = await fetch(
        `https://cdn.jsdelivr.net/npm/${name}@${resolved.version}/expo-module.config.json`
      );
      if (!response.ok) throw new Error(`Cannot read ${name} expo-module.config.json.`);
      const text = await response.text();
      try {
        moduleConfig = JSON.parse(text);
      } catch {
        moduleConfig = { platforms: null, unparsed: 'expo-module.config.json is not JSON' };
      }
    }
    // Evidence is every published file the classifier looks at: native
    // directories, podspecs and expo-module.config.json. Other files cannot
    // change a status, so they are not stored.
    const evidence = {
      files: files.filter((/** @type {string} */ f) =>
        /^\/(ios|android|apple|cpp|common\/cpp)\/|\.podspec$|^\/expo-module\.config\.json$/.test(f)
      ),
      moduleConfig: moduleConfig ? { platforms: moduleConfig.platforms ?? null } : null,
    };
    rows.push({
      name,
      range,
      resolved: resolved.version,
      ...classifyPackage(name, evidence),
      evidence,
    });
    process.stderr.write('.');
  }
  process.stderr.write('\n');
  const counts = rows.reduce(
    (/** @type {Record<string, number>} */ acc, row) => (
      (acc[row.status] = (acc[row.status] ?? 0) + 1),
      acc
    ),
    {}
  );
  return {
    schema: SCHEMA,
    sdk: 58,
    expoVersion,
    generatedBy: 'scripts/expo-sdk58-support.mjs --write',
    evidenceScope: 'Published package contents via jsDelivr. No status is a device run.',
    counts,
    packages: rows,
  };
}

/** @param {any} committed @param {{expoVersion: string, bundled: Record<string, string>}} sdk */
export function driftProblems(committed, sdk) {
  const problems = [];
  if (committed.schema !== SCHEMA) problems.push(`schema is not ${SCHEMA}`);
  if (committed.expoVersion !== sdk.expoVersion)
    problems.push(
      `list was generated for expo ${committed.expoVersion}; installed is ${sdk.expoVersion}`
    );
  const listed = new Map(committed.packages.map((/** @type {any} */ p) => [p.name, p]));
  for (const [name, range] of Object.entries(sdk.bundled)) {
    const row = listed.get(name);
    if (!row) problems.push(`${name} is in the SDK but missing from the list`);
    else if (row.range !== range) problems.push(`${name} range ${row.range} is now ${range}`);
  }
  for (const name of listed.keys())
    if (!(name in sdk.bundled)) problems.push(`${name} is listed but no longer in the SDK`);
  for (const row of committed.packages) {
    const expected = classifyPackage(row.name, row.evidence);
    if (expected.status !== row.status)
      problems.push(
        `${row.name} is ${row.status} but its evidence classifies as ${expected.status}`
      );
  }
  return problems;
}

async function main() {
  const mode = process.argv[2];
  if (mode === '--write') {
    const list = await generate();
    await writeFile(SUPPORT_FILE, JSON.stringify(list, null, 2) + '\n');
    console.log(`Wrote ${path.relative(root, SUPPORT_FILE)}: ${JSON.stringify(list.counts)}`);
  } else if (mode === '--check') {
    const committed = JSON.parse(await readFile(SUPPORT_FILE, 'utf8'));
    const problems = driftProblems(committed, await installedSdk());
    if (problems.length) {
      console.error(
        `Expo SDK 58 support list drifted:\n  ${problems.join('\n  ')}\nRegenerate: node scripts/expo-sdk58-support.mjs --write`
      );
      process.exitCode = 1;
    } else console.log(`Expo SDK 58 support list matches expo ${committed.expoVersion}.`);
  } else {
    console.error('Use --write or --check.');
    process.exitCode = 2;
  }
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url))
  await main();
