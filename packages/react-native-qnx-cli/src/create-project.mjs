import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import {
  cp,
  lstat,
  mkdir,
  mkdtemp,
  readFile,
  readdir,
  realpath,
  rm,
  writeFile,
} from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { parseArgs } from 'node:util';
import { CONSUMER_CONTRACT } from './consumer-contract.mjs';
import { validateAppIdentity } from './prepare-native.mjs';

const sourceRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
const help = [
  'React Native QNX - create a shared Expo application',
  '  project create --project <NEW-directory> --app-id org.example.notes --name Notes',
  '                 [--component-name main] [--with-marathon] [--dry-run]',
  '',
  'QNX is the default target. --with-marathon adds a native Linux/Wayland target.',
  'This creates source and locally packed platform packages. It does not install,',
  'build, deploy, publish, or qualify any native executable.',
].join('\n');

/** @param {unknown} error */
function absent(error) {
  return error instanceof Error && 'code' in error && error.code === 'ENOENT';
}

/** @param {string} file */
async function smallSource(file) {
  const stat = await lstat(file);
  if (!stat.isFile() || stat.isSymbolicLink() || stat.size > 1024 * 1024) {
    throw new Error('Invalid bounded local source file.');
  }
  return readFile(file, 'utf8');
}

/** @param {string} source @param {string} dest @param {{files: number, bytes: number}} budget */
async function copySource(source, dest, budget) {
  const stat = await lstat(source);
  if (stat.isSymbolicLink()) throw new Error('Package source symlinks are not supported.');
  if (stat.isDirectory()) {
    await mkdir(dest, { recursive: true });
    for (const child of (await readdir(source)).sort()) {
      await copySource(path.join(source, child), path.join(dest, child), budget);
    }
    return;
  }
  if (!stat.isFile() || stat.size > 2 * 1024 * 1024) {
    throw new Error('Invalid package source file.');
  }
  budget.files += 1;
  budget.bytes += stat.size;
  if (budget.files > 1024 || budget.bytes > 16 * 1024 * 1024) {
    throw new Error('Package source budget exceeded.');
  }
  await cp(source, dest, { force: false, errorOnExist: true });
}

/** @param {string} output @param {boolean} marathon */
async function packLocalPackages(output, marathon) {
  const names = ['react-native-qnx', 'metro-config-qnx'];
  if (marathon) names.push('react-native-marathon', 'metro-config-marathon');
  const staging = await mkdtemp(path.join(output, '.pack-'));
  const vendor = path.join(output, 'vendor');
  await mkdir(vendor);
  /** @type {Record<string, string>} */
  const expected = { ...CONSUMER_CONTRACT.packages, ...CONSUMER_CONTRACT.marathonPackages };
  /** @type {Record<string, {version: string, file: string, sha256: string}>} */
  const artifacts = {};
  const manifests = new Map();
  try {
    for (const name of names) {
      const manifest = JSON.parse(
        await smallSource(path.join(sourceRoot, 'packages', name, 'package.json'))
      );
      if (manifest.name !== '@rn-qnx/' + name || manifest.version !== expected[manifest.name]) {
        throw new Error('Platform package identity/version drift: ' + name);
      }
      manifests.set(manifest.name, manifest);
    }
    for (const name of names) {
      const packageName = '@rn-qnx/' + name;
      const manifest = structuredClone(manifests.get(packageName));
      const packageDir = path.join(staging, name);
      await mkdir(packageDir);
      const files = name.startsWith('react-native-') ? ['index.js', 'Libraries'] : ['src'];
      if (name === 'metro-config-qnx') files.push('public-entry.js', 'public-entry.d.ts');
      const budget = { files: 0, bytes: 0 };
      for (const file of files) {
        await copySource(
          path.join(sourceRoot, 'packages', name, file),
          path.join(packageDir, file),
          budget
        );
      }
      for (const field of ['dependencies', 'peerDependencies', 'optionalDependencies']) {
        for (const [dependency, version] of Object.entries(manifest[field] ?? {})) {
          if (typeof version === 'string' && version.startsWith('workspace:')) {
            const local = manifests.get(dependency);
            if (!local) throw new Error('Unpacked workspace dependency: ' + dependency);
            manifest[field][dependency] = local.version;
          }
        }
      }
      manifest.files = [...files, 'LICENSE'];
      await writeFile(
        path.join(packageDir, 'package.json'),
        JSON.stringify(manifest, null, 2) + '\n'
      );
      await writeFile(
        path.join(packageDir, 'LICENSE'),
        await smallSource(path.join(sourceRoot, 'LICENSE'))
      );
      const packed = spawnSync(
        'npm',
        ['pack', '--ignore-scripts', '--json', '--pack-destination', vendor],
        {
          cwd: packageDir,
          encoding: 'utf8',
          timeout: 30000,
          killSignal: 'SIGKILL',
          maxBuffer: 2 * 1024 * 1024,
          env: {
            ...process.env,
            npm_config_ignore_scripts: 'true',
            npm_config_audit: 'false',
            npm_config_fund: 'false',
          },
        }
      );
      if (packed.error || packed.status !== 0) {
        throw new Error('Local npm pack failed: ' + (packed.error?.message ?? packed.stderr));
      }
      const info = JSON.parse(packed.stdout);
      const file = info?.[0]?.filename;
      if (info.length !== 1 || typeof file !== 'string' || !/^[a-z0-9.-]+\.tgz$/.test(file)) {
        throw new Error('Unexpected npm archive filename.');
      }
      const archive = path.join(vendor, file);
      const stat = await lstat(archive);
      if (
        !stat.isFile() ||
        stat.isSymbolicLink() ||
        stat.size < 1 ||
        stat.size > 16 * 1024 * 1024
      ) {
        throw new Error('Invalid packed platform archive.');
      }
      artifacts[packageName] = {
        version: manifest.version,
        file: 'vendor/' + file,
        sha256: createHash('sha256')
          .update(await readFile(archive))
          .digest('hex'),
      };
    }
    return artifacts;
  } finally {
    await rm(staging, { recursive: true, force: true });
  }
}

/** @param {string[]} argv */
export async function createProjectMain(argv) {
  const { values, positionals, tokens } = parseArgs({
    args: argv,
    strict: true,
    allowPositionals: true,
    tokens: true,
    options: {
      help: { type: 'boolean', short: 'h' },
      project: { type: 'string' },
      'app-id': { type: 'string' },
      name: { type: 'string' },
      'component-name': { type: 'string' },
      'with-marathon': { type: 'boolean' },
      'dry-run': { type: 'boolean' },
    },
  });
  const seen = new Set();
  for (const token of tokens) {
    if (token.kind !== 'option') continue;
    if (seen.has(token.name)) throw new Error('Duplicate option --' + token.name);
    seen.add(token.name);
  }
  if (values.help || argv.length === 0) {
    console.log(help);
    return 0;
  }
  if (positionals.length || !values.project?.trim()) {
    throw new Error('Specify --project with a new directory.');
  }
  if (!['linux', 'darwin'].includes(process.platform)) {
    throw new Error('Local package creation currently supports Linux/macOS.');
  }
  const identity = validateAppIdentity({
    appId: values['app-id'],
    displayName: values.name,
    componentName: values['component-name'] ?? 'main',
  });
  const requested = path.resolve(values.project);
  const parent = await realpath(path.dirname(requested));
  const destination = path.join(parent, path.basename(requested));
  if (/[\x00-\x1f;"$\\]/.test(destination)) {
    throw new Error('Destination contains unsupported native build characters.');
  }
  try {
    await lstat(destination);
    throw new Error('Destination exists; existing applications cannot be overwritten.');
  } catch (error) {
    if (!absent(error)) throw error;
  }
  const marathon = !!values['with-marathon'];
  const platforms = marathon ? ['qnx', 'marathon'] : ['qnx'];
  if (values['dry-run']) {
    console.log(
      JSON.stringify(
        { action: 'dry-run', destination, identity, platforms, nativeBuildVerified: false },
        null,
        2
      )
    );
    return 0;
  }
  await mkdir(destination, { mode: 0o700 });
  const originalDestination = await lstat(destination);
  try {
    const artifacts = await packLocalPackages(destination, marathon);
    const versions = CONSUMER_CONTRACT.packages;
    /** @type {Record<string, string>} */
    const scripts = {
      'bundle:qnx':
        'metro build index.js --config metro.config.js --platform qnx --dev false --out build/qnx/app.bundle.js',
    };
    if (marathon) {
      scripts['bundle:marathon'] =
        'metro build index.js --config metro.marathon.config.js --platform marathon --dev false --out build/marathon/app.bundle.js';
    }
    const manifest = {
      name: identity.appId.toLowerCase().replaceAll('_', '-'),
      version: '0.0.1',
      private: true,
      main: 'index.js',
      packageManager: CONSUMER_CONTRACT.packageManager,
      engines: { node: '>=26.0.0' },
      scripts,
      dependencies: {
        react: versions.react,
        'react-native': versions['react-native'],
        expo: versions.expo,
        zustand: '5.1.0',
        ...Object.fromEntries(
          Object.entries(artifacts).map(([name, entry]) => [name, 'file:' + entry.file])
        ),
      },
      devDependencies: {
        metro: versions.metro,
        'metro-config': versions.metro,
        'babel-preset-expo': versions['babel-preset-expo'],
        '@react-native/metro-config': versions['@react-native/metro-config'],
      },
    };
    await writeFile(
      path.join(destination, 'package.json'),
      JSON.stringify(manifest, null, 2) + '\n'
    );
    await writeFile(
      path.join(destination, 'app.json'),
      JSON.stringify(
        {
          expo: {
            name: identity.displayName,
            slug: identity.appId.split('.').pop()?.toLowerCase(),
            extra: {
              rnqnx: { appId: identity.appId, componentName: identity.componentName, platforms },
            },
          },
        },
        null,
        2
      ) + '\n'
    );
    // Carry the reviewed pnpm overrides and patch inventory into the
    // standalone consumer instead of silently dropping source/security fixes.
    const workspace = await smallSource(path.join(sourceRoot, 'pnpm-workspace.yaml'));
    const marker = '\noverrides:\n';
    if (workspace.split(marker).length !== 2 || !workspace.includes('\npatchedDependencies:\n')) {
      throw new Error('Platform workspace patch policy changed; review before creating apps.');
    }
    const policy = workspace.slice(workspace.indexOf(marker) + 1);
    const locals = Object.entries(artifacts)
      .map(([name, entry]) => "  '" + name + "': 'file:" + entry.file + "'")
      .join('\n');
    await writeFile(
      path.join(destination, 'pnpm-workspace.yaml'),
      "packages:\n  - '.'\n\n" + policy.replace('overrides:\n', 'overrides:\n' + locals + '\n')
    );
    await mkdir(path.join(destination, 'patches'));
    const patches = [...policy.matchAll(/^  [^\n]+: (patches\/[A-Za-z0-9@._-]+\.patch)$/gm)];
    if (patches.length !== 2) {
      throw new Error('Platform patch inventory drifted; review required.');
    }
    for (const match of patches) {
      await writeFile(
        path.join(destination, match[1]),
        await smallSource(path.join(sourceRoot, match[1]))
      );
    }
    await writeFile(
      path.join(destination, 'index.js'),
      [
        "import React from 'react';",
        "import { AppRegistry, View, Text, Pressable, StyleSheet } from 'react-native';",
        "import { create } from 'zustand';",
        'const useStore = create(set => ({ count: 0, increment: () => set(s => ({ count: s.count + 1 })) }));',
        'function App() {',
        '  const count = useStore(s => s.count);',
        '  const increment = useStore(s => s.increment);',
        '  return React.createElement(View, { style: styles.root },',
        "    React.createElement(Text, { style: styles.heading }, 'React Native QNX'),",
        '    React.createElement(Pressable, { onPress: increment },',
        "      React.createElement(Text, null, 'Tap: ' + count)));",
        '}',
        'const styles = StyleSheet.create({ root: { flex: 1, padding: 28 }, heading: { fontSize: 28 } });',
        'AppRegistry.registerComponent(' + JSON.stringify(identity.componentName) + ', () => App);',
        '',
      ].join('\n')
    );
    await writeFile(
      path.join(destination, 'babel.config.js'),
      "module.exports = { presets: ['babel-preset-expo'] };\n"
    );
    /** @param {'qnx'|'marathon'} platform */
    const metroConfig = (platform) => {
      const factory = platform === 'qnx' ? 'getQnxMetroConfig' : 'getMarathonMetroConfig';
      return [
        "const path = require('node:path');",
        "const { createRequire } = require('node:module');",
        'const { ' + factory + " } = require('@rn-qnx/metro-config-" + platform + "');",
        "const withPublicEntry = require('@rn-qnx/metro-config-qnx/public-entry');",
        "const rn = createRequire(require.resolve('react-native/package.json'));",
        'module.exports = withPublicEntry(' + factory + '(__dirname, {',
        "  resolver: { extraNodeModules: { '@babel/runtime': path.dirname(rn.resolve('@babel/runtime/package.json')) } },",
        "}), '" + platform + "');",
        '',
      ].join('\n');
    };
    await writeFile(path.join(destination, 'metro.config.js'), metroConfig('qnx'));
    if (marathon) {
      await writeFile(path.join(destination, 'metro.marathon.config.js'), metroConfig('marathon'));
    }
    await writeFile(path.join(destination, '.gitignore'), 'node_modules/\nbuild/\n.pack-*/\n');
    await writeFile(
      path.join(destination, 'platform-packages.json'),
      JSON.stringify(
        {
          schemaVersion: 1,
          source: 'local-source-checkout',
          artifacts,
          published: false,
        },
        null,
        2
      ) + '\n'
    );
    console.log(
      JSON.stringify(
        {
          action: 'created',
          destination,
          identity,
          platforms,
          archives: Object.keys(artifacts),
          dependenciesInstalled: false,
          nativeBuildVerified: false,
          runtimeVerified: false,
        },
        null,
        2
      )
    );
    return 0;
  } catch (error) {
    // Only delete the uniquely created destination if nobody has replaced it.
    // Caller-created files are never adopted or overwritten.
    const current = await lstat(destination).catch(() => null);
    if (
      current?.ino === originalDestination.ino &&
      current.dev === originalDestination.dev &&
      !current.isSymbolicLink()
    ) {
      await rm(destination, { recursive: true, force: true });
    }
    throw error;
  }
}
