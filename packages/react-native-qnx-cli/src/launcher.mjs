import { createHash } from 'node:crypto';
import { lstat, mkdir, mkdtemp, readFile, realpath, rename, rm, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { crc32 } from 'node:zlib';
import { validateAppIdentity } from './prepare-native.mjs';

export const LAUNCHER_GENERATOR = 'react-native-qnx/launcher-v1';

/** @param {string} root @param {string} relative @param {boolean} [create] */
async function confined(root, relative, create = false) {
  if (
    !relative ||
    path.isAbsolute(relative) ||
    path.win32.isAbsolute(relative) ||
    /[\x00-\x1f\\]/.test(relative) ||
    relative.split('/').some((p) => !p || p === '.' || p === '..')
  )
    throw new Error('Launcher paths must be confined project-relative paths without symlinks.');
  let current = root;
  for (const [index, part] of relative.split('/').entries()) {
    current = path.join(current, part);
    const parent = index < relative.split('/').length - 1;
    let info;
    try {
      info = await lstat(current);
    } catch (error) {
      if (!(error instanceof Error && 'code' in error && error.code === 'ENOENT')) throw error;
      if (!create) throw error;
      if (parent) {
        await mkdir(current);
        info = await lstat(current);
      }
    }
    if (info?.isSymbolicLink() || (parent && !info?.isDirectory()))
      throw new Error('Launcher paths cannot contain symlinks or non-directory parents.');
  }
  return current;
}

/** @param {string} file @param {number} maximum */
async function bounded(file, maximum) {
  const info = await lstat(file);
  if (!info.isFile() || info.isSymbolicLink() || info.size < 1 || info.size > maximum)
    throw new Error('Expected a bounded regular launcher input.');
  return readFile(file);
}

/** @param {string} project */
export async function launcherIdentity(project) {
  const root = await realpath(path.resolve(project));
  const app = JSON.parse((await bounded(await confined(root, 'app.json'), 65536)).toString());
  const identity = validateAppIdentity({
    ...app?.expo?.extra?.rnqnx,
    displayName: app?.expo?.name,
  });
  return { root, app, identity, runtime: runtimeProfile(app?.expo?.extra?.rnqnx?.qnx?.runtime) };
}

/** @param {Buffer} bytes */
export function validateLauncherIcon(bytes) {
  if (
    bytes.length > 8 * 1024 * 1024 ||
    bytes.length < 45 ||
    !bytes.subarray(0, 8).equals(Buffer.from('89504e470d0a1a0a', 'hex'))
  )
    throw new Error('QNX launcher icons must be local PNG files under 8 MiB.');
  let offset = 8;
  let width = 0;
  let height = 0;
  let data = false;
  let end = false;
  while (offset + 12 <= bytes.length) {
    const length = bytes.readUInt32BE(offset);
    if (offset + length + 12 > bytes.length) throw new Error('Truncated PNG icon.');
    const kind = bytes.toString('ascii', offset + 4, offset + 8);
    if (
      crc32(bytes.subarray(offset + 4, offset + 8 + length)) !==
      bytes.readUInt32BE(offset + 8 + length)
    )
      throw new Error('PNG icon checksum mismatch.');
    if (offset === 8 && (kind !== 'IHDR' || length !== 13)) throw new Error('Invalid PNG header.');
    if (kind === 'IHDR') {
      if (offset !== 8 || length !== 13) throw new Error('Duplicate PNG header.');
      width = bytes.readUInt32BE(offset + 8);
      height = bytes.readUInt32BE(offset + 12);
    }
    if (kind === 'IDAT') data = true;
    offset += length + 12;
    if (kind === 'IEND') {
      end = length === 0 && offset === bytes.length;
      break;
    }
  }
  if (!data || !end || width !== height || width < 256 || width > 2048)
    throw new Error('Use a complete square PNG icon, 256–2048 pixels (1024 recommended).');
  return { width, height, sha256: createHash('sha256').update(bytes).digest('hex') };
}

/** @param {unknown} value @param {string} label */
export function targetPath(value, label) {
  if (
    typeof value !== 'string' ||
    !/^\/[A-Za-z0-9_.\/-]+$/.test(value) ||
    value
      .split('/')
      .slice(1)
      .some((p) => !p || p === '.' || p === '..')
  )
    throw new Error(
      `${label} must be an absolute target path without escapes or shell metacharacters.`
    );
  return value;
}

/** @param {unknown} value */
function runtimeProfile(value) {
  if (value === undefined) return { libraryPaths: /** @type {string[]} */ ([]) };
  if (!value || typeof value !== 'object' || Array.isArray(value))
    throw new Error('Invalid QNX runtime profile.');
  const profile =
    /** @type {{libraryPaths?: unknown, qtPluginDirectory?: unknown, qtFontDirectory?: unknown}} */ (
      value
    );
  if (
    Object.keys(profile).some(
      (k) => !['libraryPaths', 'qtPluginDirectory', 'qtFontDirectory'].includes(k)
    )
  )
    throw new Error('Unknown QNX runtime profile field.');
  if (
    profile.libraryPaths !== undefined &&
    (!Array.isArray(profile.libraryPaths) || profile.libraryPaths.length > 16)
  )
    throw new Error('libraryPaths must be an array of at most 16 target directories.');
  return {
    libraryPaths: /** @type {unknown[]} */ (profile.libraryPaths ?? []).map((p) =>
      targetPath(p, 'libraryPaths')
    ),
    ...(profile.qtPluginDirectory === undefined
      ? {}
      : { qtPluginDirectory: targetPath(profile.qtPluginDirectory, 'qtPluginDirectory') }),
    ...(profile.qtFontDirectory === undefined
      ? {}
      : { qtFontDirectory: targetPath(profile.qtFontDirectory, 'qtFontDirectory') }),
  };
}

/** @param {string} project */
export async function launcherConfiguration(project) {
  const { root, app, identity } = await launcherIdentity(project);
  const input = app?.expo?.extra?.rnqnx?.qnx?.icon ?? app?.expo?.icon;
  if (typeof input !== 'string')
    throw new Error('Configure expo.icon (or expo.extra.rnqnx.qnx.icon) with a local PNG.');
  const source = input.replace(/^\.\//, '');
  const icon = await bounded(await confined(root, source), 8 * 1024 * 1024);
  const image = validateLauncherIcon(icon);
  return {
    root,
    icon,
    configuration: {
      schemaVersion: 1,
      generatedBy: LAUNCHER_GENERATOR,
      ...identity,
      icon: { source, ...image },
      runtime: runtimeProfile(app?.expo?.extra?.rnqnx?.qnx?.runtime),
    },
  };
}

/** @param {string} project @param {boolean} [dryRun] */
export async function prepareLauncher(project, dryRun = false) {
  const { root, icon, configuration } = await launcherConfiguration(project);
  const output = path.join(root, 'build/qnx/launcher');
  if (!dryRun) {
    await confined(root, 'build/qnx/launcher/launcher.json', true);
    let owned = false;
    try {
      const old = JSON.parse((await bounded(path.join(output, 'launcher.json'), 65536)).toString());
      if (old.generatedBy !== LAUNCHER_GENERATOR)
        throw new Error('Refusing to replace unowned launcher metadata.');
      owned = true;
    } catch (error) {
      if (!(error instanceof Error && 'code' in error && error.code === 'ENOENT')) throw error;
    }
    if (!owned) {
      try {
        await lstat(path.join(output, 'icon.png'));
        throw new Error('Refusing to overwrite an icon without owned launcher metadata.');
      } catch (error) {
        if (!(error instanceof Error && 'code' in error && error.code === 'ENOENT')) throw error;
      }
    }
    const staging = await mkdtemp(path.join(output, '.rnqnx-stage-'));
    try {
      for (const [name, bytes] of Object.entries({
        'launcher.json': JSON.stringify(configuration, null, 2) + '\n',
        'icon.png': icon,
      })) {
        const file = await confined(root, `build/qnx/launcher/${name}`, true);
        try {
          await lstat(file);
          await bounded(path.join(output, 'launcher.json'), 65536);
        } catch (error) {
          if (!(error instanceof Error && 'code' in error && error.code === 'ENOENT')) throw error;
        }
        const temporary = path.join(staging, name);
        await writeFile(temporary, bytes, { flag: 'wx', mode: 0o600 });
        await rename(temporary, file);
      }
    } finally {
      await rm(staging, { recursive: true, force: true });
    }
  }
  return { output, configuration };
}

/** @param {string} value */
export function shellQuote(value) {
  return "'" + value.replaceAll("'", "'\\''") + "'";
}

/** @param {Awaited<ReturnType<typeof launcherConfiguration>>['configuration']} config
 * @param {string} appDirectory @param {string} registry @param {string} [executable] */
export function launcherFiles(config, appDirectory, registry, executable = config.appId) {
  validateAppIdentity(config);
  targetPath(appDirectory, 'app directory');
  targetPath(registry, 'launcher registry');
  if (!/^[A-Za-z0-9][A-Za-z0-9._-]*$/.test(executable) || executable === '.' || executable === '..')
    throw new Error('Executable must be a filename, not a command or path.');
  const entry = `${registry}/apps/${config.appId}`;
  const profile = config.runtime;
  const wrapper = [
    '#!/bin/sh',
    'set -eu',
    'test "$(id -u)" -ne 0 || { echo "React Native applications must not run as root" >&2; exit 1; }',
    `cd ${shellQuote(appDirectory)}`,
    `export LD_LIBRARY_PATH=${shellQuote([appDirectory, ...profile.libraryPaths].join(':'))}`,
    'export QT_QPA_PLATFORM=qnx',
    ...(profile.qtPluginDirectory
      ? [
          `export QT_PLUGIN_PATH=${shellQuote(profile.qtPluginDirectory)}`,
          `export QT_QPA_PLATFORM_PLUGIN_PATH=${shellQuote(profile.qtPluginDirectory + '/platforms')}`,
        ]
      : []),
    ...(profile.qtFontDirectory
      ? [`export QT_QPA_FONTDIR=${shellQuote(profile.qtFontDirectory)}`]
      : []),
    `exec ${shellQuote(appDirectory + '/' + executable)} --fullscreen`,
    '',
  ].join('\n');
  const displayName = config.displayName.replaceAll('\\', '\\\\');
  return {
    'launch.sh': wrapper,
    'app.desktop': `[Desktop Entry]\nType=Application\nName=${displayName}\nExec=${entry}/launch.sh\nIcon=${entry}/icon.png\nTerminal=false\nSingleton=Y\nCategories=Development\nX-RNQNX-Generated-By=${LAUNCHER_GENERATOR}\n`,
    'entry.conf': `\n[Launcher]\nDesktopFileName=${config.appId}.desktop\nBackgroundIcon=/usr/share/icons/grey_large.png\nDisplayName=true\nFontSize=16\nRow=2\nWidth=120\nHeight=120\nBackgroundHeight=140\nBackgroundWidth=150\nOrder=20\n`,
    owner: `${LAUNCHER_GENERATOR}:${config.appId}\n`,
  };
}
