import { createHash } from 'node:crypto';
import { createReadStream } from 'node:fs';
import {
  copyFile,
  lstat,
  mkdir,
  mkdtemp,
  open,
  readFile,
  realpath,
  rename,
  rm,
} from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { parseArgs } from 'node:util';
import {
  generateIconSet,
  hostIndependentDescriptor,
  identityToolPath,
  measureTileLabel,
  notMeasured,
  resolveTileLabel,
  validateIconSource,
  verifyPreparedIdentity,
} from './app-identity.mjs';
import { boundedFile, projectPath } from './project-path.mjs';

/** @typedef {import('./app-identity.mjs').IconSet} IconSet */
/** @typedef {import('./app-identity.mjs').LabelFit} LabelFit */
/** @typedef {{schemaVersion: number, generatedBy: string, appId: string, displayName: string, componentName: string, platform: string, bundle: string, bundleSha256: string}} BaseConfiguration */
/** @typedef {BaseConfiguration & {shortName: string, icon: IconSet, labelFit: LabelFit}} NativeConfiguration */
const help = `React Native QNX native consumer configuration
  project prepare-native --project <directory> [--platform qnx|marathon]
                         [--bundle build/<platform>/app.bundle.js]
                         [--identity-tool <path> | --skip-identity-tool]
                         [--dry-run]

Reads static app.json: expo.name, expo.icon and
expo.extra.rnqnx.{appId,componentName,icon,shortName}.
The icon must be a square PNG of at least 1024x1024. The tile label
(shortName, else expo.name) must fit the target launcher on one line.
--identity-tool (or RNQNX_IDENTITY_TOOL) names the Qt host tool built from
native/app-identity that scales the icon set and measures the label. It
builds on Linux and macOS. Elsewhere, --skip-identity-tool still checks the
icon file and label text but writes no icon set and records the label as not
measured; the app then runs with the platform's default icon.
Writes build/<platform>/icons/ and build/<platform>/native-app.json for the
existing native CMake backend.
An identical rerun does not write. Changed generated output is not overwritten:
remove only that generated file after reviewing your app/bundle change, then rerun.
This does not install packages, compile native code or deploy to a device.
`;

/** @param {unknown} value @param {string} label */
function text(value, label) {
  if (typeof value !== 'string' || !value || /[\uD800-\uDFFF]/u.test(value))
    throw new Error(`${label} must be a nonempty, well-formed string.`);
  return value;
}
/** @param {unknown} value */
export function validateAppIdentity(value) {
  const object = /** @type {{appId?: unknown, displayName?: unknown, componentName?: unknown}} */ (
    value
  );
  if (!object || typeof object !== 'object') throw new Error('Missing application identity.');
  const appId = text(object.appId, 'appId');
  const displayName = text(object.displayName, 'expo.name');
  const componentName = text(object.componentName, 'componentName');
  if (
    appId.length > 255 ||
    /[^A-Za-z0-9_.]/.test(appId) ||
    !/^[A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z][A-Za-z0-9_]*)+$/.test(appId) ||
    appId.endsWith('.desktop')
  )
    throw new Error('appId must be a reverse-DNS identifier without a .desktop suffix.');
  if (/[^A-Za-z0-9_]/.test(componentName) || !/^[A-Za-z][A-Za-z0-9_]{0,127}$/.test(componentName))
    throw new Error('componentName must be an ASCII identifier of 1-128 characters.');
  if (
    displayName.length > 128 ||
    displayName.trim() !== displayName ||
    /[\x00-\x1f\x7f-\x9f\u2028\u2029]/.test(displayName)
  )
    throw new Error(
      'expo.name must be 1-128 characters without control characters or surrounding whitespace.'
    );
  return { appId, displayName, componentName };
}

/** Identity and bundle fields only; the icon set and tile label are added by
 * prepareNativeMain and re-checked by verifyPreparedIdentity.
 * @param {string} project @param {string} platform @param {string} [relativeBundle] */
export async function nativeConfiguration(project, platform = 'qnx', relativeBundle) {
  if (!['qnx', 'marathon'].includes(platform)) throw new Error('Use platform qnx or marathon.');
  if (!project.trim()) throw new Error('Project directory must not be empty.');
  const root = await realpath(path.resolve(project));
  if (/[\x00-\x1f;"$\\]/.test(root))
    throw new Error('Project path contains unsupported CMake characters.');
  const configFile = await projectPath(root, 'app.json');
  await boundedFile(configFile, 65536);
  const app = JSON.parse(await readFile(configFile, 'utf8'));
  const identity = validateAppIdentity({
    ...app?.expo?.extra?.rnqnx,
    displayName: app?.expo?.name,
  });
  const relative = relativeBundle ?? `build/${platform}/app.bundle.js`;
  if (!relative.startsWith(`build/${platform}/`))
    throw new Error('Bundle must be below the selected build/<platform>/ directory.');
  const bundle = await projectPath(root, relative);
  await boundedFile(bundle, 256 * 1024 * 1024);
  const hash = createHash('sha256');
  for await (const chunk of createReadStream(bundle)) hash.update(chunk);
  /** @type {BaseConfiguration} */
  const configuration = {
    schemaVersion: 1,
    generatedBy: 'react-native-qnx/prepare-native-v1',
    ...identity,
    platform,
    bundle,
    bundleSha256: hash.digest('hex'),
  };
  return { root, configuration, app };
}

/** @param {string} root @param {import('./app-identity.mjs').ExpoConfig} expo
 * @param {string} platform @param {string | undefined} tool @param {boolean} skip
 * @param {string} staging
 * @returns {Promise<{shortName: string, icon: IconSet, labelFit: LabelFit}>} */
async function identityAssets(root, expo, platform, tool, skip, staging) {
  const source = await validateIconSource(root, expo);
  const tile = resolveTileLabel(expo);
  if (skip) {
    const sha256 = createHash('sha256')
      .update(await readFile(source.file))
      .digest('hex');
    return {
      shortName: tile.label,
      icon: { source: source.file, sha256, sizes: [] },
      labelFit: notMeasured(platform, 'Skipped with --skip-identity-tool; no label was measured.'),
    };
  }
  const labelFit = measureTileLabel(() => identityToolPath(tool), platform, tile);
  const icons = path.join(root, 'build', platform, 'icons');
  const icon = await generateIconSet(identityToolPath(tool), source.file, staging, (size) =>
    path.join(icons, `icon-${size}.png`)
  );
  return { shortName: tile.label, icon, labelFit };
}

/** Move generated icons into build/<platform>/icons without following a
 * symlink planted at any destination: each file lands under a fresh temporary
 * name in the same directory and is renamed over the leaf.
 * @param {string} root @param {string} platform @param {string} staging
 * @param {IconSet} icon */
async function stageIcons(root, platform, staging, icon) {
  if (!icon.sizes.length) return;
  const directory = await projectPath(root, `build/${platform}/icons`);
  await mkdir(directory, { recursive: true });
  if (!(await lstat(directory)).isDirectory() || (await lstat(directory)).isSymbolicLink())
    throw new Error(`${directory} must be a real directory.`);
  for (const entry of icon.sizes) {
    try {
      const leaf = await lstat(entry.path);
      if (!leaf.isFile() || leaf.isSymbolicLink())
        throw new Error(
          `${entry.path} is a symlink or not a regular file. Remove it and prepare again; icons are never written through links.`
        );
    } catch (error) {
      if (!(error instanceof Error && 'code' in error && error.code === 'ENOENT')) throw error;
    }
    const temporary = path.join(directory, `.${path.basename(entry.path)}.${process.pid}.tmp`);
    await copyFile(path.join(staging, path.basename(entry.path)), temporary, 1 /* COPYFILE_EXCL */);
    await rename(temporary, entry.path);
  }
}

/** @param {string[]} argv */
export async function prepareNativeMain(argv) {
  const { values, positionals, tokens } = parseArgs({
    args: argv,
    strict: true,
    allowPositionals: true,
    tokens: true,
    options: {
      help: { type: 'boolean', short: 'h' },
      project: { type: 'string' },
      platform: { type: 'string' },
      bundle: { type: 'string' },
      'identity-tool': { type: 'string' },
      'skip-identity-tool': { type: 'boolean' },
      'dry-run': { type: 'boolean' },
    },
  });
  const seen = new Set();
  for (const token of tokens)
    if (token.kind === 'option') {
      if (seen.has(token.name)) throw new Error(`Duplicate option --${token.name}.`);
      seen.add(token.name);
    }
  if (values.help || argv.length === 0) {
    console.log(help);
    return 0;
  }
  if (positionals.length) throw new Error('prepare-native does not accept positional arguments.');
  const dryRun = Boolean(values['dry-run']);
  const skip = Boolean(values['skip-identity-tool']);
  if (skip && values['identity-tool'])
    throw new Error('Use either --identity-tool or --skip-identity-tool, not both.');
  const {
    root,
    configuration: base,
    app,
  } = await nativeConfiguration(values.project ?? '.', values.platform ?? 'qnx', values.bundle);
  const relative = `build/${base.platform}/native-app.json`;
  const output = await projectPath(root, relative, !dryRun);
  const staging = await mkdtemp(
    dryRun ? path.join(tmpdir(), 'rnqnx-icons-') : path.join(path.dirname(output), '.icons-')
  );
  /** @type {NativeConfiguration} */
  let configuration;
  let action = dryRun ? 'dry-run' : 'created';
  try {
    configuration = {
      ...base,
      ...(await identityAssets(
        root,
        app.expo,
        base.platform,
        values['identity-tool'],
        skip,
        staging
      )),
    };
    if (skip)
      console.error(
        'rnqnx: warning: --skip-identity-tool: no icon set was generated and the tile label was not measured. The app will use the platform default icon.'
      );
    if (!dryRun) {
      let existing;
      try {
        await boundedFile(output, 65536);
        existing = JSON.parse(await readFile(output, 'utf8'));
      } catch (error) {
        if (!(error instanceof Error && 'code' in error && error.code === 'ENOENT')) throw error;
      }
      if (existing) {
        // Compare only what every host computes the same way; then check the
        // icons already on disk against the hashes the existing file names.
        if (
          JSON.stringify(hostIndependentDescriptor(existing)) !==
          JSON.stringify(hostIndependentDescriptor(configuration))
        )
          throw new Error(
            'Generated native configuration differs; review and remove only native-app.json before preparing again.'
          );
        await verifyPreparedIdentity(root, app.expo, existing);
        configuration = existing;
        action = 'unchanged';
      } else {
        await stageIcons(root, base.platform, staging, configuration.icon);
        const handle = await open(output, 'wx', 0o600);
        try {
          await handle.writeFile(JSON.stringify(configuration, null, 2) + '\n');
        } catch (error) {
          await rm(output);
          throw error;
        } finally {
          await handle.close();
        }
      }
    }
  } finally {
    await rm(staging, { recursive: true, force: true });
  }
  console.log(
    JSON.stringify(
      { action, output, configuration, nativeBuildVerified: false, runtimeVerified: false },
      null,
      2
    )
  );
  return 0;
}
