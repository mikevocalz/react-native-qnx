import { readFile, access, writeFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';
import { format } from 'prettier';

/** @param {string} file */
async function json(file) {
  return JSON.parse(await readFile(file, 'utf8'));
}

/** @param {string} root */
export async function dependencyBaseline(root) {
  const manifest = await json(path.join(root, 'package.json'));
  const require = createRequire(path.join(root, 'package.json'));
  const direct = manifest.devDependencies;
  for (const [name, version] of Object.entries(direct)) {
    const installed = await json(require.resolve(`${name}/package.json`));
    if (installed.version !== version) {
      throw new Error(
        `${name}: installed ${installed.version} disagrees with package.json ${version}`
      );
    }
  }
  const expoRequire = createRequire(require.resolve('expo/package.json'));
  const rnPath = require.resolve('react-native/package.json');
  const rn = await json(rnPath);
  return {
    reactNative: direct['react-native'],
    react: direct.react,
    expo: direct.expo,
    metro: direct.metro,
    reactNativeMetroConfig: direct['@react-native/metro-config'],
    babelPresetExpo: direct['babel-preset-expo'],
    nitroCore: direct['nitro-core'],
    nitroModules: direct['react-native-nitro-modules'],
    nitrogen: direct.nitrogen,
    typescript: direct.typescript,
    codegenTypescript: direct['@typescript/typescript6'],
    node: manifest.engines.node,
    packageManager: manifest.packageManager,
    expoModulesCore: (await json(expoRequire.resolve('expo-modules-core/package.json'))).version,
    expoMetro: (await json(expoRequire.resolve('@expo/metro/package.json'))).version,
    expoMetroConfig: (await json(expoRequire.resolve('@expo/metro-config/package.json'))).version,
    hermesCompiler: rn.dependencies['hermes-compiler'],
    hermesSource: (
      await readFile(path.join(path.dirname(rnPath), 'sdks/.hermesv1version'), 'utf8')
    ).trim(),
  };
}

/** @param {string} root @param {string[]} paths */
async function checkEvidence(root, paths) {
  if (!Array.isArray(paths)) throw new Error('Evidence must be an array of repository paths');
  for (const file of paths) {
    if (typeof file !== 'string' || !file.startsWith('docs/evidence/') || file.includes('..')) {
      throw new Error(`Evidence must be a local docs/evidence path: ${file}`);
    }
    await access(path.join(root, file));
  }
}

/** @param {string} value */
function cell(value) {
  return value.replaceAll('|', '\\|').replaceAll('\n', ' ');
}

/** @param {string} root @param {boolean} [write] @param {boolean} [release] */
export async function syncDocs(root, write = false, release = false) {
  const baseline = await dependencyBaseline(root);
  const support = await json(path.join(root, 'docs/component-support.json'));
  const record = await json(path.join(root, 'docs/releases/v1-readiness.json'));
  if (
    support.schemaVersion !== 1 ||
    !Array.isArray(support.components) ||
    !support.components.length
  ) {
    throw new Error('Invalid component support schema');
  }
  if (record.schemaVersion !== 1 || !Array.isArray(record.gates) || !record.gates.length) {
    throw new Error('Invalid v1 readiness schema');
  }
  const names = new Set();
  const counts = new Map();
  const rows = [];
  for (const component of support.components) {
    if (
      typeof component.name !== 'string' ||
      !/^[A-Za-z][A-Za-z0-9]*$/.test(component.name) ||
      names.has(component.name)
    ) {
      throw new Error(`Invalid or duplicate component: ${component.name}`);
    }
    names.add(component.name);
    if (!['supported', 'partial', 'no-op', 'unverified', 'unsupported'].includes(component.host)) {
      throw new Error(`Invalid host status: ${component.name}`);
    }
    const devices = [];
    for (const lane of ['pi400', 'pi4Qsti']) {
      if (!['scoped', 'unverified', 'failed'].includes(component[lane])) {
        throw new Error(`Invalid device status: ${component.name}/${lane}`);
      }
      if (component[lane] !== 'unverified' && !component.evidence?.[lane]?.length) {
        throw new Error(`Device result requires evidence: ${component.name}/${lane}`);
      }
      await checkEvidence(root, component.evidence?.[lane]);
      const evidence = component.evidence[lane]
        .map((/** @type {string} */ p) => `[receipt](${path.posix.relative('docs', p)})`)
        .join(', ');
      devices.push(`${component[lane]}${evidence ? ` (${evidence})` : ''}`);
    }
    if (typeof component.notes !== 'string' || !component.notes.trim()) {
      throw new Error(`Missing component limits: ${component.name}`);
    }
    counts.set(component.host, (counts.get(component.host) ?? 0) + 1);
    rows.push(
      `| \`${component.name}\` | ${component.host} | ${devices.join(' | ')} | ${cell(component.notes)} |`
    );
  }
  const ids = new Set();
  for (const gate of record.gates) {
    if (
      typeof gate.id !== 'string' ||
      ids.has(gate.id) ||
      !['blocked', 'pending', 'passed'].includes(gate.status) ||
      typeof gate.acceptance !== 'string' ||
      !gate.acceptance.trim() ||
      typeof gate.title !== 'string'
    ) {
      throw new Error(`Invalid or duplicate release gate: ${gate.id}`);
    }
    ids.add(gate.id);
    await checkEvidence(root, gate.evidence);
    if (gate.status === 'passed' && !gate.evidence.length) {
      throw new Error(`Passed release gate requires evidence: ${gate.id}`);
    }
  }
  const requiredGates = [
    'distribution',
    'ci',
    'cold-boot',
    'device-parity',
    'performance',
    'developer-path',
    'documentation',
  ];
  if (ids.size !== requiredGates.length || requiredGates.some((id) => !ids.has(id))) {
    throw new Error('The v1 record must retain every required release gate.');
  }
  const state = {
    schemaVersion: 2,
    project: 'react-native-qnx',
    generatedBy: 'pnpm docs:sync',
    dependencyBaseline: baseline,
    evidenceBaseline: record.evidenceBaseline,
    activeMilestone: 'v1 preparation; release gates remain open',
    releaseGates: record.gates.map((/** @type {{id: string, status: string}} */ gate) => ({
      id: gate.id,
      status: gate.status,
    })),
    historicalState: 'docs/handoff/HISTORY_THROUGH_2026_11_07.md',
  };
  const dependencies =
    '| Component | Pinned / resolved version |\n| --- | --- |\n' +
    Object.entries(baseline)
      .map(([name, version]) => `| ${name} | \`${version}\` |`)
      .join('\n');
  const components =
    '| Component | Host | Pi 400 / custom image | Pi 4 QSTI | Behavior and limits |\n| --- | --- | --- | --- | --- |\n' +
    rows.join('\n') +
    `\n\n${names.size} components: ` +
    [...counts].map(([status, count]) => `${count} ${status}`).join(', ') +
    '.';
  const gates =
    '| Gate | Status | Acceptance evidence required |\n| --- | --- | --- |\n' +
    record.gates
      .map(
        (/** @type {{title: string, status: string, acceptance: string}} */ gate) =>
          `| ${cell(gate.title)} | ${gate.status} | ${cell(gate.acceptance)} |`
      )
      .join('\n');
  const outputs = new Map([
    [
      '.project-state.json',
      await format(JSON.stringify(state), { parser: 'json', printWidth: 100 }),
    ],
  ]);
  for (const [file, key, block] of [
    ['README.md', 'dependencies', dependencies],
    ['docs/getting-started/compatibility-matrix.md', 'dependencies', dependencies],
    ['docs/component-support.md', 'components', components],
    ['docs/releases/v1-readiness.md', 'gates', gates],
  ]) {
    const source = await readFile(path.join(root, file), 'utf8');
    const start = `<!-- generated:${key}:start -->`;
    const end = `<!-- generated:${key}:end -->`;
    if (
      source.split(start).length !== 2 ||
      source.split(end).length !== 2 ||
      source.indexOf(end) < source.indexOf(start)
    ) {
      throw new Error(`Missing or duplicate generated markers: ${file}`);
    }
    const updated =
      source.slice(0, source.indexOf(start) + start.length) +
      '\n\n' +
      block +
      '\n\n' +
      source.slice(source.indexOf(end));
    outputs.set(
      file,
      await format(updated, { parser: 'markdown', printWidth: 100, proseWrap: 'preserve' })
    );
  }
  for (const [file, expected] of outputs) {
    if (write) await writeFile(path.join(root, file), expected);
    else if ((await readFile(path.join(root, file), 'utf8')) !== expected) {
      throw new Error(`${file} is stale. Run pnpm docs:sync and review the generated diff.`);
    }
  }
  const documents = [...outputs.keys()]
    .filter((file) => file.endsWith('.md'))
    .concat([
      'docs/handoff/CURRENT_STATE.md',
      'docs/getting-started/qnx-cli.md',
      'docs/policy/premium-components.md',
      'docs/adr/index.md',
      'THIRD_PARTY_NOTICES.md',
      'SECURITY.md',
      'CODE_OF_CONDUCT.md',
    ]);
  for (const file of documents) {
    const source = await readFile(path.join(root, file), 'utf8');
    for (const match of source.matchAll(/\]\(([^)]+)\)/g)) {
      const target = match[1].split('#')[0];
      if (!target || /^[a-z]+:/i.test(target)) continue;
      await access(path.resolve(root, path.dirname(file), decodeURIComponent(target)));
    }
  }
  if (release) {
    const open = record.gates.filter(
      (/** @type {{status: string}} */ gate) => gate.status !== 'passed'
    );
    if (open.length)
      throw new Error(
        `v1 is not release-ready:\n${open.map((/** @type {{id: string, status: string, title: string}} */ gate) => `  ${gate.id} ${gate.status}: ${gate.title}`).join('\n')}`
      );
  }
  return { components: names.size, gates: record.gates.length };
}
