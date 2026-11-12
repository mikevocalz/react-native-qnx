import { lstat, mkdir, readFile, rename, rm, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { parseArgs } from 'node:util';
import { boundedFile, projectPath } from './project-path.mjs';

const help = `React Native QNX desktop launcher tile
  project desktop-entry --project <directory> --exec <device script>
                        --app-dir <device directory> [--run-as <user>]
                        [--row 1|2] [--order <n>] [--dry-run]

Reads build/qnx/native-app.json (written by project prepare-native with the
identity tool) and writes, under build/qnx/desktop/:

  <appId>.desktop     the tile: Name (the tile label), Exec, Icon, Singleton=Y
  launcher-tile.conf  the [Launcher] block to append to desktop.conf
  install-tile.sh     copies both into /system/etc/desktop_files as root,
                      keeping desktop.conf.before-rnqnx-tile as a backup

--exec is the absolute path of the script that starts the app on the device.
--app-dir is the absolute device directory that holds the app's icons/; the
tile uses icons/icon-128.png, the size of the stock QNX tiles.
The tile goes in row 1 after the stock Developer Desktop, Terminal and Readme
tiles (orders 1 to 3), drawn like them with its name; --row and --order move it.
--run-as starts the script as that user through /usr/bin/sudo. The launcher
runs as root on the QNX 8.0 Raspberry Pi image, so without it the app runs as
root too.
The QNX desktop launcher (demolauncher) reads tiles only when it starts.
This does not copy anything to a device.
`;

const DEVICE_PATH = /^\/(?:[A-Za-z0-9_.+-]+\/)*[A-Za-z0-9_.+-]+$/;
export const TILE_ICON_SIZE = 128;

/** @param {string} value @param {string} label */
function devicePath(value, label) {
  if (!DEVICE_PATH.test(value) || value.split('/').some((part) => part === '.' || part === '..'))
    throw new Error(
      `${label} must be an absolute device path of letters, digits, '_', '.', '+' and '-' only.`
    );
  return value;
}

/** demolauncher reads key=value lines and rejects a second '=' on a line.
 * @param {string} value @param {string} label */
function entryValue(value, label) {
  if (typeof value !== 'string' || !value || /[=\x00-\x1f\x7f\u2028\u2029]/u.test(value))
    throw new Error(`${label} must be nonempty without '=' or control characters.`);
  return value;
}

/** Desktop entry and launcher block for one app.
 * @param {{appId: string, shortName?: string, displayName: string, icon?: {sizes?: {size: number}[]}}} descriptor
 * @param {{exec: string, appDir: string, runAs?: string, row?: number, order?: number}} options */
export function desktopEntry(descriptor, options) {
  const appId = descriptor.appId;
  if (
    typeof appId !== 'string' ||
    !/^[A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z][A-Za-z0-9_]*)+$/.test(appId)
  )
    throw new Error('native-app.json has no valid appId; run project prepare-native.');
  const name = entryValue(descriptor.shortName ?? descriptor.displayName, 'The tile label');
  const sizes = (descriptor.icon?.sizes ?? []).map((entry) => entry.size);
  if (!sizes.includes(TILE_ICON_SIZE))
    throw new Error(
      `native-app.json has no ${TILE_ICON_SIZE} px icon. Run project prepare-native with the identity tool.`
    );
  const script = devicePath(options.exec, '--exec');
  if (options.runAs !== undefined && !/^[a-z_][a-z0-9_-]{0,31}$/.test(options.runAs))
    throw new Error('--run-as must be a user name of lowercase letters, digits, _ and -.');
  const exec = options.runAs ? `/usr/bin/sudo -u ${options.runAs} ${script}` : script;
  const appDir = devicePath(options.appDir, '--app-dir');
  const row = options.row ?? 1;
  const order = options.order ?? 4;
  if (![1, 2].includes(row)) throw new Error('--row must be 1 or 2.');
  if (!Number.isInteger(order) || order < 1 || order > 99)
    throw new Error('--order must be an integer from 1 to 99.');
  const fileName = `${appId}.desktop`;
  const icon = `${appDir}/icons/icon-${TILE_ICON_SIZE}.png`;
  const desktop = [
    '[Desktop Entry]',
    `Name=${name}`,
    `Exec=${exec}`,
    `Icon=${icon}`,
    // A second click switches to the running window instead of starting
    // another copy.
    'Singleton=Y',
    'Categories=Utilities',
    '',
  ].join('\n');
  const launcher = [
    '',
    `# ${appId}, added by react-native-qnx project desktop-entry`,
    '[Launcher]',
    `DesktopFileName=${fileName}`,
    'BackgroundIcon=/usr/share/icons/grey_large.png',
    'DisplayName=true',
    `Row=${row}`,
    'Width=120',
    'Height=120',
    'BackgroundHeight=140',
    'BackgroundWidth=150',
    `Order=${order}`,
    '',
  ].join('\n');
  const install = `#!/bin/sh
# Installs the ${appId} tile for the QNX desktop launcher. Run as root on the
# device from the directory that holds this script and its two files.
set -e
D=/system/etc/desktop_files
F=${fileName}
here=$(dirname "$0")
cp "$here/$F" "$D/$F"
chmod 644 "$D/$F"
if ! grep -q "^DesktopFileName=$F\\$" "$D/desktop.conf"; then
  [ -f "$D/desktop.conf.before-rnqnx-tile" ] || cp "$D/desktop.conf" "$D/desktop.conf.before-rnqnx-tile"
  cat "$here/launcher-tile.conf" >> "$D/desktop.conf"
fi
echo "Installed $D/$F. The launcher reads tiles when it starts:"
echo "  slay demolauncher; on -d demolauncher -a $D"
`;
  return { fileName, desktop, launcher, install, icon, exec, name };
}

/** Write without following a planted symlink at the leaf.
 * @param {string} file @param {string} content @param {number} mode */
async function writeLeaf(file, content, mode) {
  try {
    const stat = await lstat(file);
    if (!stat.isFile() || stat.isSymbolicLink())
      throw new Error(`${file} is not a regular file; remove it and run again.`);
  } catch (error) {
    if (!(error instanceof Error && 'code' in error && error.code === 'ENOENT')) throw error;
  }
  const temporary = path.join(path.dirname(file), `.${path.basename(file)}.${process.pid}.tmp`);
  try {
    await writeFile(temporary, content, { mode, flag: 'wx' });
    await rename(temporary, file);
  } finally {
    await rm(temporary, { force: true });
  }
}

/** @param {string[]} argv */
export async function desktopEntryMain(argv) {
  const { values, positionals } = parseArgs({
    args: argv,
    strict: true,
    allowPositionals: true,
    options: {
      help: { type: 'boolean', short: 'h' },
      project: { type: 'string' },
      exec: { type: 'string' },
      'app-dir': { type: 'string' },
      'run-as': { type: 'string' },
      row: { type: 'string' },
      order: { type: 'string' },
      'dry-run': { type: 'boolean' },
    },
  });
  if (values.help || argv.length === 0) {
    console.log(help);
    return 0;
  }
  if (positionals.length) throw new Error('desktop-entry does not accept positional arguments.');
  if (!values.exec || !values['app-dir']) throw new Error('Pass --exec and --app-dir.');
  const root = path.resolve(values.project ?? '.');
  const descriptorPath = await projectPath(root, 'build/qnx/native-app.json');
  await boundedFile(descriptorPath, 65536);
  const descriptor = JSON.parse(await readFile(descriptorPath, 'utf8'));
  const entry = desktopEntry(descriptor, {
    exec: values.exec,
    appDir: values['app-dir'],
    runAs: values['run-as'],
    row: values.row === undefined ? undefined : Number(values.row),
    order: values.order === undefined ? undefined : Number(values.order),
  });
  const dryRun = Boolean(values['dry-run']);
  const files = {
    [entry.fileName]: [entry.desktop, 0o644],
    'launcher-tile.conf': [entry.launcher, 0o644],
    'install-tile.sh': [entry.install, 0o755],
  };
  const directory = await projectPath(root, 'build/qnx/desktop', !dryRun);
  if (!dryRun) {
    await mkdir(directory, { recursive: true });
    for (const [name, [content, mode]] of Object.entries(files))
      await writeLeaf(
        path.join(directory, name),
        /** @type {string} */ (content),
        /** @type {number} */ (mode)
      );
  }
  console.log(
    JSON.stringify(
      {
        action: dryRun ? 'dry-run' : 'written',
        directory,
        files: Object.keys(files),
        tile: { name: entry.name, exec: entry.exec, icon: entry.icon },
      },
      null,
      2
    )
  );
  return 0;
}
