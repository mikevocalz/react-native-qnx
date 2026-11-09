import { createHash } from 'node:crypto';
import { createReadStream } from 'node:fs';
import { lstat, mkdir, open, readFile, realpath, rm } from 'node:fs/promises';
import path from 'node:path';
import { parseArgs } from 'node:util';

/** @typedef {{schemaVersion: number, generatedBy: string, appId: string, displayName: string, componentName: string, platform: string, bundle: string, bundleSha256: string}} NativeConfiguration */
const help = `React Native QNX native consumer configuration
  project prepare-native --project <directory> [--platform qnx|marathon]
                         [--bundle build/<platform>/app.bundle.js] [--dry-run]

Reads static app.json: expo.name and expo.extra.rnqnx.{appId,componentName}.
Writes build/<platform>/native-app.json for the existing native CMake backend.
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

/** @param {unknown} error */
function absent(error) {
  return error instanceof Error && 'code' in error && error.code === 'ENOENT';
}
/** @param {string} root @param {string} relative @param {boolean} [createParents] */
async function projectPath(root, relative, createParents = false) {
  if (
    !relative ||
    /[\x00-\x1f\\;"$]/.test(relative) ||
    path.isAbsolute(relative) ||
    path.win32.isAbsolute(relative) ||
    relative.split('/').some((part) => !part || part === '.' || part === '..')
  )
    throw new Error(
      'Use a project-relative path without escapes, control characters or CMake metacharacters.'
    );
  let current = root;
  const parts = relative.split('/');
  for (const [index, part] of parts.entries()) {
    current = path.join(current, part);
    const parent = index < parts.length - 1;
    let stat;
    try {
      stat = await lstat(current);
    } catch (error) {
      if (!absent(error)) throw error;
      if (parent && createParents) {
        try {
          await mkdir(current);
        } catch (created) {
          if (!(created instanceof Error && 'code' in created && created.code === 'EEXIST'))
            throw created;
        }
        stat = await lstat(current);
      } else continue;
    }
    if (stat.isSymbolicLink() || (parent && !stat.isDirectory()))
      throw new Error('Consumer paths cannot contain symlinks or non-directory parents.');
  }
  return current;
}
/** @param {string} file @param {number} maximum */
async function boundedFile(file, maximum) {
  const stat = await lstat(file);
  if (!stat.isFile() || stat.isSymbolicLink() || stat.size < 1 || stat.size > maximum)
    throw new Error('Expected a nonempty regular file within the size limit.');
  return stat;
}

/** @param {string} project @param {string} platform @param {string} [relativeBundle] */
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
  /** @type {NativeConfiguration} */
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
  const { root, configuration, app } = await nativeConfiguration(
    values.project ?? '.',
    values.platform ?? 'qnx',
    values.bundle
  );
  const launcherIcon =
    configuration.platform === 'qnx' && (app?.expo?.icon || app?.expo?.extra?.rnqnx?.qnx?.icon);
  // Validate the launcher icon before writing any descriptor.
  if (launcherIcon) await (await import('./launcher.mjs')).launcherConfiguration(root);
  const relative = `build/${configuration.platform}/native-app.json`;
  const output = await projectPath(root, relative, !values['dry-run']);
  const encoded = JSON.stringify(configuration, null, 2) + '\n';
  let action = values['dry-run'] ? 'dry-run' : 'created';
  if (!values['dry-run']) {
    try {
      const handle = await open(output, 'wx', 0o600);
      try {
        await handle.writeFile(encoded);
      } catch (error) {
        await rm(output);
        throw error;
      } finally {
        await handle.close();
      }
    } catch (error) {
      if (!(error instanceof Error && 'code' in error && error.code === 'EEXIST')) throw error;
      await boundedFile(output, 65536);
      if ((await readFile(output, 'utf8')) !== encoded)
        throw new Error(
          'Generated native configuration differs; review and remove only native-app.json before preparing again.'
        );
      action = 'unchanged';
    }
  }
  const launcher =
    launcherIcon
      ? await (await import('./launcher.mjs')).prepareLauncher(root, !!values['dry-run'])
      : null;
  console.log(
    JSON.stringify(
      {
        action,
        output,
        configuration,
        launcher,
        nativeBuildVerified: false,
        runtimeVerified: false,
      },
      null,
      2
    )
  );
  return 0;
}
