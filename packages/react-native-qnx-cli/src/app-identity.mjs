import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { existsSync } from 'node:fs';
import { open, readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { crc32 } from 'node:zlib';
import { boundedFile, projectPath } from './project-path.mjs';

/** Validation and generation for the app's icon and launcher tile label.
 * Callers pass the resolved Expo config object (the `expo` value of app.json,
 * or `exp` from @expo/config); this module never reads or evaluates app config
 * files itself. */

/** @typedef {{name?: unknown, icon?: unknown, extra?: {rnqnx?: {icon?: unknown, shortName?: unknown}}}} ExpoConfig */
/** @typedef {{size: number, path: string, sha256: string}} IconFile */
/** @typedef {{source: string, sha256: string, sizes: IconFile[]}} IconSet */
/** @typedef {{width: number, height: number, bitDepth: number, colorType: number, interlaced: boolean}} PngHeader */
/** @typedef {{id: string, name: string, source: string, fonts: string[], fontNames: string[], pixelSize: number, boxWidthPx: number, marginPx: number, lineCount: 1}} LauncherProfile */
/** labelFit. `widthPx` and `fallbackCodePoints` depend on the host's Qt and
 * fallback fonts; prepare-native leaves them out when comparing reruns.
 * @typedef {{target: string, status: 'measured', launcher: string, launcherSource: string,
 * fontNames: string[], pixelSize: number, boxWidthPx: number, marginPx: number, lineCount: number,
 * widthPx: number, fallbackCodePoints: string[], deviceMeasured: false}
 * | {target: string, status: 'not-measured', reason: string, deviceMeasured: false}} LabelFit */

export const MINIMUM_ICON_PX = 1024;
export const MAXIMUM_ICON_PX = 8192;
/** Generated edges. 16-48 cover title bars and task switchers at 1x-2x; 64-256
 * cover launcher tiles (Marathon's Keel iconSizeLauncher is 86 px at pixel
 * ratio 1 and 128 px at 1.5); 512 covers a 2x tile at ratio 3. */
export const ICON_SIZES = Object.freeze([16, 24, 32, 48, 64, 96, 128, 192, 256, 512]);
const MAXIMUM_ICON_BYTES = 32 * 1024 * 1024;
const PNG_SIGNATURE = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]);
const sourceRoot = fileURLToPath(new URL('../../../', import.meta.url));
const fontDirectory = path.join(sourceRoot, 'native', 'app-identity', 'fonts');
/** Used when neither expo.icon nor expo.extra.rnqnx.icon is set, as Expo does. */
export const DEFAULT_ICON = path.join(sourceRoot, 'template', 'qnx', 'assets', 'icon.png');
export const DEFAULT_ICON_NOTICE =
  'rnqnx: notice: no app icon is set, so the default icon template/qnx/assets/icon.png is used. Set expo.icon to a square PNG of at least 1024x1024 to use your own.';

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
    // Room for device rendering differences the host measurement cannot see.
    marginPx: 2,
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

/** Which icon the app uses. When neither field is set the default template
 * icon is used (`defaulted: true`); a field that is set is never replaced.
 * @param {ExpoConfig} config */
export function iconSetting(config) {
  const override = config?.extra?.rnqnx?.icon;
  const icon = override ?? config?.icon;
  const field = override == null ? 'expo.icon' : 'expo.extra.rnqnx.icon';
  if (icon == null)
    return { field: 'default icon', relative: null, declared: DEFAULT_ICON, defaulted: true };
  if (typeof icon !== 'string' || !icon)
    throw new Error(
      `${field} must be a project-relative path such as "./assets/icon.png", pointing at a square PNG of at least 1024x1024.`
    );
  return { field, relative: icon.replace(/^\.\//, ''), declared: icon };
}

/** Check the icon source without decoding it: the configured one, or the
 * default template icon when none is configured.
 * @param {string} root @param {ExpoConfig} config */
export async function validateIconSource(root, config) {
  const setting = iconSetting(config);
  const { field, relative, declared } = setting;
  const defaulted = setting.defaulted === true;
  let file;
  try {
    file = relative === null ? DEFAULT_ICON : await projectPath(root, relative);
    await boundedFile(file, MAXIMUM_ICON_BYTES);
  } catch (error) {
    throw new Error(
      defaulted
        ? `No app icon is set and the default icon ${DEFAULT_ICON} is not readable (${reason(error)}). Set expo.icon to a square PNG of at least 1024x1024.`
        : `${field} "${declared}" is not a readable file inside the project (${reason(error)}). Point it at a square PNG of at least 1024x1024.`
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
  if (header.width > MAXIMUM_ICON_PX)
    throw new Error(
      `${field} "${declared}" is ${header.width}x${header.height}. Export a square PNG no larger than ${MAXIMUM_ICON_PX}x${MAXIMUM_ICON_PX}.`
    );
  return { field, file, defaulted, ...header };
}

/** True when a ZERO WIDTH JOINER at `index` sits between two pictographs, as in
 * an emoji sequence such as woman + ZWJ + laptop.
 * @param {string[]} chars @param {number} index */
function joinsPictographs(chars, index) {
  const pictograph = /^\p{Extended_Pictographic}$/u;
  let before = index - 1;
  while (before >= 0 && /^[\uFE0F\p{Emoji_Modifier}]$/u.test(chars[before])) before--;
  return before >= 0 && pictograph.test(chars[before]) && pictograph.test(chars[index + 1] ?? '');
}

/** Tile label: extra.rnqnx.shortName when set, otherwise expo.name. expo.name
 * stays the title bar and About name.
 * @param {ExpoConfig} config */
export function resolveTileLabel(config) {
  const shortName = config?.extra?.rnqnx?.shortName;
  const field = shortName === undefined ? 'expo.name' : 'expo.extra.rnqnx.shortName';
  const label = shortName ?? config?.name;
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
  // Invisible format characters (zero-width space, bidi overrides such as
  // U+202E, soft hyphen, tags) change how a label reads without being seen.
  // The only one allowed is ZERO WIDTH JOINER inside an emoji sequence.
  const chars = [...label];
  for (const [index, char] of chars.entries()) {
    if (/^\p{Cf}$/u.test(char) && !(char === '\u200D' && joinsPictographs(chars, index))) {
      const code = char.codePointAt(0)?.toString(16).toUpperCase().padStart(4, '0');
      throw new Error(
        `${field} contains the invisible format character U+${code}. Remove it; tile labels allow only visible text and emoji.`
      );
    }
  }
  if (!/[^\p{Z}\p{C}\p{M}]/u.test(label))
    throw new Error(`${field} has no visible character. Use a label people can read.`);
  return { field, label };
}

/** @param {unknown} error */
function reason(error) {
  return error instanceof Error ? error.message.replace(/\.$/, '') : String(error);
}

const IDENTITY_TOOL_HOSTS = ['linux', 'darwin'];
const BUILD_HINT =
  'It needs CMake 3.31.9 or later and Qt 6.11.1 or later (Core and Gui); point CMAKE_PREFIX_PATH at Qt if CMake cannot find it. Or pass --identity-tool with a tool built on another machine of the same OS.';

/** Where prepare-native builds rnqnx_identity_tool when no tool is given.
 * RNQNX_IDENTITY_TOOL_BUILD_DIR moves it (tests use this). */
export function identityToolBuildDirectory() {
  return path.resolve(
    process.env.RNQNX_IDENTITY_TOOL_BUILD_DIR || path.join(sourceRoot, 'build', 'identity-tool')
  );
}

/** Find the tool without building it: --identity-tool, then
 * RNQNX_IDENTITY_TOOL, then an earlier automatic build.
 * @param {string | undefined} explicit
 * @returns {{path: string, origin: string} | null} */
export function locateIdentityTool(explicit) {
  if (explicit) return { path: path.resolve(explicit), origin: '--identity-tool' };
  if (process.env.RNQNX_IDENTITY_TOOL)
    return { path: path.resolve(process.env.RNQNX_IDENTITY_TOOL), origin: 'RNQNX_IDENTITY_TOOL' };
  const built = path.join(identityToolBuildDirectory(), 'rnqnx_identity_tool');
  return existsSync(built) ? { path: built, origin: 'automatic build' } : null;
}

/** The error for a host where the tool cannot be built. */
export function unsupportedIdentityHost(platform = process.platform) {
  return `Cannot generate the app icon set on this ${platform} host: rnqnx_identity_tool, the Qt program that scales the icon, builds only on Linux and macOS. Run prepare-native on a Linux or macOS host, or pass --identity-tool with a tool built for this host.`;
}

/** Return the tool, building it from native/app-identity with CMake when it
 * is missing. Every prepared app gets a generated icon set, so there is no
 * path that continues without the tool.
 * @param {string | undefined} explicit @param {(line: string) => void} [log] */
export function ensureIdentityTool(explicit, log = (line) => console.error(line)) {
  const found = locateIdentityTool(explicit);
  if (found) {
    if (!existsSync(found.path))
      throw new Error(`The identity tool from ${found.origin} does not exist: ${found.path}.`);
    return found.path;
  }
  if (!IDENTITY_TOOL_HOSTS.includes(process.platform)) throw new Error(unsupportedIdentityHost());
  const directory = identityToolBuildDirectory();
  const source = path.join(sourceRoot, 'native', 'app-identity');
  log(`rnqnx: building rnqnx_identity_tool in ${directory} (first run only).`);
  for (const args of [
    ['-S', source, '-B', directory, '-DRNQNX_BUILD_IDENTITY_TOOL=ON', '-DCMAKE_BUILD_TYPE=Release'],
    ['--build', directory, '--target', 'rnqnx_identity_tool'],
  ]) {
    const result = spawnSync('cmake', args, {
      encoding: 'utf8',
      timeout: 15 * 60_000,
      maxBuffer: 16 * 1024 * 1024,
    });
    if (result.error || result.status !== 0) {
      const output = (result.error?.message ?? `${result.stdout}\n${result.stderr}`)
        .trim()
        .split('\n')
        .slice(-8)
        .join('\n');
      throw new Error(
        `Cannot build rnqnx_identity_tool, which generates the app icon set; "cmake ${args[0]}" failed:\n${output}\n${BUILD_HINT} No native-app.json was written.`
      );
    }
  }
  const built = path.join(directory, 'rnqnx_identity_tool');
  if (!existsSync(built))
    throw new Error(`CMake finished without producing ${built}. ${BUILD_HINT}`);
  return built;
}

/** The label fit recorded when the tool is skipped or cannot measure.
 * @param {string} platform @param {string} reason @returns {LabelFit} */
export function notMeasured(platform, reason) {
  return { target: platform, status: 'not-measured', reason, deviceMeasured: false };
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
  if (!('fonts' in profile)) return notMeasured(platform, profile.reason);
  if (!profile.fonts.every((font) => existsSync(font)))
    return notMeasured(
      platform,
      `The ${profile.name} fonts are not installed with this CLI (expected under native/app-identity/fonts in a source checkout).`
    );
  const measured = runTool(tool(), [
    'label',
    '--text-hex',
    Buffer.from(tile.label, 'utf8').toString('hex'),
    ...profile.fonts.flatMap((font) => ['--font', font]),
    '--pixel-size',
    String(profile.pixelSize),
    '--box-width',
    String(profile.boxWidthPx),
    '--margin',
    String(profile.marginPx),
  ]);
  if (measured.lineCount !== 1 || typeof measured.widthPx !== 'number')
    throw new Error('The label tool returned an unexpected measurement.');
  if (measured.mightBeRichText)
    throw new Error(
      `Tile label ${quoted(tile.label)} (${tile.field}) looks like markup. The ${profile.name} shows labels with QML's automatic text format, so tags such as <b> would render as formatting instead of the measured text. Remove the markup from the label.`
    );
  if (!measured.fits) {
    const fix = measured.suggestion
      ? `Set expo.extra.rnqnx.shortName to a shorter label, for example ${quoted(measured.suggestion)}.`
      : 'Set expo.extra.rnqnx.shortName to a shorter label.';
    throw new Error(
      `Tile label ${quoted(tile.label)} (${tile.field}) is ${measured.overflowPx.toFixed(1)} px too wide for the ${profile.name}: ` +
        `it measures ${measured.widthPx.toFixed(1)} px in ${profile.fontNames.join(' / ')} at ${profile.pixelSize} px, ` +
        `and the label box is ${profile.boxWidthPx} px wide on one line with ${profile.marginPx} px kept free. ${fix} expo.name stays the window title.`
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
    marginPx: profile.marginPx,
    lineCount: profile.lineCount,
    widthPx: measured.widthPx,
    fallbackCodePoints: measured.fallbackCodePoints ?? [],
    deviceMeasured: false,
  };
}

/** The parts of a descriptor that must match between reruns on different
 * hosts. Icon pixel hashes and measured label widths depend on the host's Qt
 * version and font engine, so they are left out here and checked against the
 * files on disk by verifyPreparedIdentity instead.
 * @param {Record<string, unknown> & {icon?: IconSet, labelFit?: LabelFit}} descriptor */
export function hostIndependentDescriptor(descriptor) {
  const { icon, labelFit, ...rest } = descriptor;
  /** @type {Record<string, unknown>} */
  const stable = { ...rest };
  if (icon)
    stable.icon = {
      source: icon.source,
      sha256: icon.sha256,
      sizes: icon.sizes.map((entry) => ({ size: entry.size, path: entry.path })),
    };
  if (labelFit) {
    const copy = /** @type {Record<string, unknown>} */ ({ ...labelFit });
    delete copy.widthPx;
    delete copy.fallbackCodePoints;
    stable.labelFit = copy;
  }
  return stable;
}

/** Re-check a prepared descriptor's icon and tile label against the current
 * config and files without running the tool. Throws when anything drifted,
 * including a descriptor without the full generated icon set.
 * @param {string} root @param {ExpoConfig} config
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
