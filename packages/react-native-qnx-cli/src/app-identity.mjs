import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { open, readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { crc32 } from 'node:zlib';
import { boundedFile, projectPath } from './project-path.mjs';

/** Validation and generation for the app's icon and launcher tile label.
 * prepare-native and doctor pass an already-resolved Expo config object here;
 * this module never reads or evaluates app config files itself. */

/** @typedef {{expo?: {name?: unknown, icon?: unknown, extra?: {rnqnx?: {icon?: unknown, shortName?: unknown}}}}} ResolvedAppConfig */
/** @typedef {{size: number, path: string, sha256: string}} IconFile */
/** @typedef {{source: string, sha256: string, sizes: IconFile[]}} IconSet */
/** @typedef {{width: number, height: number, bitDepth: number, colorType: number, interlaced: boolean}} PngHeader */
/** @typedef {{id: string, name: string, source: string, fonts: string[], fontNames: string[], pixelSize: number, boxWidthPx: number, lineCount: 1}} LauncherProfile */
/** @typedef {{target: string, status: 'measured', launcher: string, launcherSource: string,
 * fontNames: string[], pixelSize: number, boxWidthPx: number, lineCount: number,
 * widthPx: number, fallbackCodePoints: string[], deviceMeasured: false}
 * | {target: string, status: 'not-measured', reason: string, deviceMeasured: false}} LabelFit */

export const MINIMUM_ICON_PX = 1024;
/** Generated edges. 16-48 cover title bars and task switchers at 1x-2x; 64-256
 * cover launcher tiles (Marathon's Keel iconSizeLauncher is 86 px at pixel
 * ratio 1 and 128 px at 1.5); 512 covers a 2x tile at ratio 3. */
export const ICON_SIZES = Object.freeze([16, 24, 32, 48, 64, 96, 128, 192, 256, 512]);
const MAXIMUM_ICON_BYTES = 32 * 1024 * 1024;
const PNG_SIGNATURE = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]);
const fontDirectory = fileURLToPath(
  new URL('../../../native/app-identity/fonts/', import.meta.url)
);

/** Launcher label boxes this repository can measure. Each is derived from the
 * named upstream source, not from a device; see docs/getting-started/app-identity.md.
 * @type {Readonly<Record<string, LauncherProfile | {id: string, reason: string}>>} */
export const LAUNCHER_PROFILES = Object.freeze({
  marathon: {
    id: 'marathon-shell-567f1fa',
    name: 'Marathon launcher',
    source:
      'patrickjquinn/Marathon-Shell@567f1faa8bfdf31611dd401134c293daa160221c shell/qml/components/MarathonAppGrid.qml',
    fonts: [
      path.join(fontDirectory, 'source-sans-3', 'SourceSans3-Regular.ttf'),
      path.join(fontDirectory, 'source-sans-3', 'SourceSans3-Light.ttf'),
    ],
    fontNames: ['Source Sans 3 Regular', 'Source Sans 3 Light'],
    pixelSize: 20,
    boxWidthPx: 92.4,
    lineCount: 1,
  },
  qnx: {
    id: 'qnx-none',
    reason:
      'No QNX launcher is defined in this repository or the QNX SDP, so there is no label box to measure.',
  },
});

/** Read and check only the PNG signature and IHDR chunk.
 * @param {string} file @returns {Promise<PngHeader>} */
export async function readPngHeader(file) {
  const handle = await open(file, 'r');
  try {
    const bytes = Buffer.alloc(33);
    const { bytesRead } = await handle.read(bytes, 0, 33, 0);
    if (bytesRead < 33 || !bytes.subarray(0, 8).equals(PNG_SIGNATURE))
      throw new Error('it is not a PNG file');
    if (bytes.readUInt32BE(8) !== 13 || bytes.toString('latin1', 12, 16) !== 'IHDR')
      throw new Error('its PNG header (IHDR) is missing');
    if (crc32(bytes.subarray(12, 29)) !== bytes.readUInt32BE(29))
      throw new Error('its PNG header (IHDR) is corrupt');
    return {
      width: bytes.readUInt32BE(16),
      height: bytes.readUInt32BE(20),
      bitDepth: bytes[24],
      colorType: bytes[25],
      interlaced: bytes[28] === 1,
    };
  } finally {
    await handle.close();
  }
}

/** @param {ResolvedAppConfig} config */
export function iconSetting(config) {
  const override = config?.expo?.extra?.rnqnx?.icon;
  const icon = override ?? config?.expo?.icon;
  const field = override === undefined ? 'expo.icon' : 'expo.extra.rnqnx.icon';
  if (typeof icon !== 'string' || !icon)
    throw new Error(
      'No app icon is set. Add "icon": "./assets/icon.png" under expo in app.json, pointing at a square PNG of at least 1024x1024.'
    );
  return { field, relative: icon.replace(/^\.\//, ''), declared: icon };
}

/** Check the configured icon source without decoding it.
 * @param {string} root @param {ResolvedAppConfig} config */
export async function validateIconSource(root, config) {
  const { field, relative, declared } = iconSetting(config);
  let file;
  try {
    file = await projectPath(root, relative);
    await boundedFile(file, MAXIMUM_ICON_BYTES);
  } catch (error) {
    throw new Error(
      `${field} "${declared}" is not a readable file inside the project (${reason(error)}). Point it at a square PNG of at least 1024x1024.`
    );
  }
  let header;
  try {
    header = await readPngHeader(file);
  } catch (error) {
    throw new Error(
      `${field} "${declared}" was rejected: ${reason(error)}. Export the icon as a square PNG of at least 1024x1024.`
    );
  }
  if (header.width !== header.height || header.width < MINIMUM_ICON_PX)
    throw new Error(
      `${field} "${declared}" is ${header.width}x${header.height}. Export a square PNG of at least ${MINIMUM_ICON_PX}x${MINIMUM_ICON_PX}; keep corners square, the platform applies any mask.`
    );
  return { field, file, ...header };
}

/** Tile label: extra.rnqnx.shortName when set, otherwise expo.name. expo.name
 * stays the title bar and About name.
 * @param {ResolvedAppConfig} config */
export function resolveTileLabel(config) {
  const shortName = config?.expo?.extra?.rnqnx?.shortName;
  const field = shortName === undefined ? 'expo.name' : 'expo.extra.rnqnx.shortName';
  const label = shortName ?? config?.expo?.name;
  if (
    typeof label !== 'string' ||
    !label ||
    label.trim() !== label ||
    /[\uD800-\uDFFF]/u.test(label) ||
    /[\x00-\x1f\x7f-\x9f\u2028\u2029]/.test(label) ||
    label.length > 128
  )
    throw new Error(
      `${field} must be 1-128 characters on one line without surrounding whitespace.`
    );
  return { field, label };
}

/** @param {unknown} error */
function reason(error) {
  return error instanceof Error ? error.message.replace(/\.$/, '') : String(error);
}

/** @param {string | undefined} explicit */
export function identityToolPath(explicit) {
  const tool = explicit ?? process.env.RNQNX_IDENTITY_TOOL;
  if (!tool)
    throw new Error(
      'The icon and label tool is not available. Build it with "cmake -S native/app-identity -B build/identity-tool -DRNQNX_BUILD_IDENTITY_TOOL=ON && cmake --build build/identity-tool", then pass --identity-tool build/identity-tool/rnqnx_identity_tool or set RNQNX_IDENTITY_TOOL.'
    );
  return path.resolve(tool);
}

/** @param {string} tool @param {string[]} args */
function runTool(tool, args) {
  const result = spawnSync(tool, args, {
    encoding: 'utf8',
    timeout: 60_000,
    maxBuffer: 1024 * 1024,
    env: { ...process.env, QT_QPA_PLATFORM: process.env.QT_QPA_PLATFORM || 'offscreen' },
  });
  if (result.error) throw new Error(`Cannot run ${tool}: ${result.error.message}`);
  if (result.status !== 0)
    throw new Error((result.stderr || `${tool} exited with ${result.status}`).trim());
  return JSON.parse(result.stdout);
}

/** @param {string} file */
async function sha256(file) {
  return createHash('sha256')
    .update(await readFile(file))
    .digest('hex');
}

/** Generate the icon set into `directory` and hash every file.
 * @param {string} tool @param {string} source @param {string} directory
 * @param {(size: number) => string} finalPath where the file will live after staging
 * @returns {Promise<IconSet>} */
export async function generateIconSet(tool, source, directory, finalPath) {
  const output = runTool(tool, [
    'icon',
    '--source',
    source,
    '--out-dir',
    directory,
    ...ICON_SIZES.flatMap((size) => ['--size', String(size)]),
  ]);
  const sizes = [];
  for (const size of ICON_SIZES) {
    const entry = output?.sizes?.find((/** @type {{size: number}} */ item) => item.size === size);
    if (!entry || path.resolve(entry.path) !== path.join(directory, `icon-${size}.png`))
      throw new Error(`The icon tool did not produce icon-${size}.png.`);
    const header = await readPngHeader(entry.path);
    if (header.width !== size || header.height !== size)
      throw new Error(`The icon tool wrote icon-${size}.png at the wrong size.`);
    sizes.push({ size, path: finalPath(size), sha256: await sha256(entry.path) });
  }
  return { source, sha256: await sha256(source), sizes };
}

/** @param {string} text */
function quoted(text) {
  return JSON.stringify(text);
}

/** Measure the tile label for one target. Throws with the measured overflow
 * when it does not fit on one line; never truncates.
 * @param {() => string} tool resolves the tool path only when a profile needs it
 * @param {string} platform @param {{field: string, label: string}} tile
 * @returns {LabelFit} */
export function measureTileLabel(tool, platform, tile) {
  const profile = LAUNCHER_PROFILES[platform];
  if (!profile) throw new Error(`No launcher profile for ${platform}.`);
  if (!('fonts' in profile))
    return {
      target: platform,
      status: 'not-measured',
      reason: profile.reason,
      deviceMeasured: false,
    };
  const measured = runTool(tool(), [
    'label',
    '--text-hex',
    Buffer.from(tile.label, 'utf8').toString('hex'),
    ...profile.fonts.flatMap((font) => ['--font', font]),
    '--pixel-size',
    String(profile.pixelSize),
    '--box-width',
    String(profile.boxWidthPx),
  ]);
  if (measured.lineCount !== 1 || typeof measured.widthPx !== 'number')
    throw new Error('The label tool returned an unexpected measurement.');
  if (!measured.fits) {
    const fix = measured.suggestion
      ? `Set expo.extra.rnqnx.shortName to a shorter label, for example ${quoted(measured.suggestion)}.`
      : 'Set expo.extra.rnqnx.shortName to a shorter label.';
    throw new Error(
      `Tile label ${quoted(tile.label)} (${tile.field}) is ${measured.overflowPx.toFixed(1)} px too wide for the ${profile.name}: ` +
        `it measures ${measured.widthPx.toFixed(1)} px in ${profile.fontNames.join(' / ')} at ${profile.pixelSize} px, ` +
        `and the label box is ${profile.boxWidthPx} px wide on one line. ${fix} expo.name stays the window title.`
    );
  }
  return {
    target: platform,
    status: 'measured',
    launcher: profile.id,
    launcherSource: profile.source,
    fontNames: profile.fontNames,
    pixelSize: profile.pixelSize,
    boxWidthPx: profile.boxWidthPx,
    lineCount: profile.lineCount,
    widthPx: measured.widthPx,
    fallbackCodePoints: measured.fallbackCodePoints ?? [],
    deviceMeasured: false,
  };
}

/** Re-check a prepared descriptor's icon and tile label against the current
 * config and files without running the tool. Throws when anything drifted.
 * @param {string} root @param {ResolvedAppConfig} config
 * @param {{icon?: IconSet, shortName?: unknown}} descriptor */
export async function verifyPreparedIdentity(root, config, descriptor) {
  const stale =
    'Native descriptor is stale: the app icon or tile label changed. Run project prepare-native.';
  const source = await validateIconSource(root, config);
  const icon = descriptor.icon;
  if (
    !icon ||
    icon.source !== source.file ||
    icon.sha256 !== (await sha256(source.file)) ||
    descriptor.shortName !== resolveTileLabel(config).label ||
    !Array.isArray(icon.sizes) ||
    icon.sizes.map((entry) => entry.size).join() !== ICON_SIZES.join()
  )
    throw new Error(stale);
  for (const entry of icon.sizes) {
    const file = await projectPath(root, path.relative(root, entry.path));
    await boundedFile(file, MAXIMUM_ICON_BYTES);
    if (file !== entry.path || (await sha256(file)) !== entry.sha256) throw new Error(stale);
  }
}
