import { createHash } from 'node:crypto';
import { mkdir, readFile } from 'node:fs/promises';
import path from 'node:path';
import { parseArgs } from 'node:util';
import { devicePath, writeLeaf } from './desktop-entry.mjs';
import { boundedFile, projectPath } from './project-path.mjs';

/** The Marathon launcher entry for a consumer app.
 *
 * Format source. Marathon-Shell lists a native (non-QML) app on its home grid
 * from a freedesktop `.desktop` file, read by `read_app` in
 * `crates/marathon-shell-native/src/desktop.rs` (patrickjquinn/Marathon-Shell
 * at 567f1faa8bfdf31611dd401134c293daa160221c, the revision the label box in
 * docs/getting-started/app-identity.md comes from; the file is unchanged at
 * 1f845160, the revision the VM contracts were read at). The signed
 * `.marathon` manifest cannot carry a native executable: its schema requires a
 * `.qml` entryPoint (docs/marathon/UPSTREAM_RFC.md). This writes the `.desktop`
 * entry, not a `.marathon` manifest.
 *
 * What the reader does with each key:
 *   file name   the id is the file stem, so `<appId>.desktop` gives id `<appId>`
 *   Type        must be Application when present
 *   Name        the tile label (`app.name`); the grid shows it verbatim
 *   Exec        run through `clean_exec`, ported below as marathonCleanExec
 *   Icon        an absolute path to a file is used as is; the file must exist
 *               when the entry is scanned, or the icon falls back
 *   X-Purism-FormFactor, X-KDE-FormFactors, Categories
 *               with the default "filter mobile friendly apps" setting
 *               (`filterMobileFriendlyApps`, default true) an entry shows on
 *               the grid only if one of these says phone, or its id is in the
 *               Store's phone list. X-Purism-FormFactor=Mobile; is written.
 *   StartupWMClass
 *               not read by desktop.rs. Written because both hand-written
 *               entries in this repository carry it; that the host reports
 *               this app id on the Wayland toplevel is not verified.
 */

/** Edge of the icon the entry points at. The launcher draws a tile 86 px wide
 * at Keel pixel ratio 1 and 128 px at 1.5 (docs/getting-started/app-identity.md),
 * and the QNX tile uses the same file. */
export const MARATHON_ENTRY_ICON_SIZE = 128;

const WINDOW_FLAGS = [
  '--new-window',
  '-new-window',
  '--new-tab',
  '-new-tab',
  '--new-instance',
  '-new-instance',
];

// `X11_ONLY` in desktop.rs: entries whose command is one of these are dropped.
const X11_ONLY = [
  'xterm',
  'uxterm',
  'xclock',
  'xcalc',
  'xeyes',
  'xlogo',
  'xev',
  'xinit',
  'xrdb',
  'xfontsel',
  'xkbcomp',
  'xkill',
  'xset',
  'xsetbg',
  'xsetroot',
  'xrandr',
  'xdpyinfo',
];

/** @param {string} value */
const fileName = (value) => value.split('/').pop() ?? value;

/** Port of `clean_exec` in desktop.rs (Marathon-Shell 567f1faa): the command
 * the compositor runs for an entry's `Exec`, or null where the entry is not
 * launched. Returns the same strings as the Rust function for the cases in its
 * unit tests, which tests/js/marathon-entry.test.mjs repeats.
 * @param {string} exec @returns {string | null} */
export function marathonCleanExec(exec) {
  const stripped = exec.replace(/%[fFuUdDnNickvm]/g, '');
  let cleaned = stripped
    .split(/\s+/)
    .filter((token) => token && !WINDOW_FLAGS.includes(token))
    .join(' ');
  if (!cleaned) return null;
  if (cleaned.startsWith('gapplication launch ')) {
    const parts = cleaned.slice('gapplication launch '.length).trim().split('.');
    if (parts.length >= 3)
      return `${parts[parts.length - 2].toLowerCase()}-${parts[parts.length - 1].toLowerCase()}`;
    if (parts.length === 2) return parts[1].toLowerCase();
    return cleaned;
  }
  if (cleaned.includes('--gapplication-service')) return null;
  if (cleaned.startsWith('/usr/bin/flatpak run '))
    cleaned = `flatpak run ${cleaned.slice('/usr/bin/flatpak run '.length)}`;
  if (cleaned.startsWith('flatpak run ')) return `FLATPAK:${cleaned}`;
  if (cleaned.startsWith('snap run ') || cleaned.startsWith('/snap/bin/')) return `SNAP:${cleaned}`;
  if (cleaned.startsWith('/')) {
    const space = cleaned.indexOf(' ');
    const binary = space < 0 ? cleaned : cleaned.slice(0, space);
    const args = space < 0 ? '' : cleaned.slice(space + 1);
    return `${fileName(binary)} ${args}`.trimEnd();
  }
  return cleaned;
}

/** Refuse an Exec that Marathon would rewrite, drop or hand to another host,
 * so the command in the entry is the command that runs. Characters beyond
 * letters, digits and `_ . / : = + @ , -` are refused as a precaution: the
 * reader does no shell quoting, and nothing in the source says what runs the
 * command.
 * @param {unknown} value */
function execValue(value) {
  if (typeof value !== 'string' || !value.trim())
    throw new Error('Pass --exec: the command Marathon starts, for example "sh /path/to/run.sh".');
  if (/[\x00-\x1f\x7f\u2028\u2029]/u.test(value))
    throw new Error('--exec contains control characters.');
  if (/%[fFuUdDnNickvm]/.test(value))
    throw new Error(
      '--exec contains a field code (%U and the like); Marathon deletes them, so the command would change.'
    );
  if (/^\s|\s$|\s{2}|\t/.test(value))
    throw new Error('--exec has leading, trailing or repeated whitespace; Marathon collapses it.');
  const flag = value.split(' ').find((token) => WINDOW_FLAGS.includes(token));
  if (flag) throw new Error(`--exec contains ${flag}; Marathon removes that argument.`);
  if (value.includes('gapplication'))
    throw new Error('--exec uses gapplication; Marathon rewrites or drops those commands.');
  if (/^(?:\/usr\/bin\/)?flatpak run /.test(value))
    throw new Error('--exec starts with "flatpak run"; Marathon runs it as a Flatpak app.');
  if (/^(?:snap run |\/snap\/bin\/)/.test(value))
    throw new Error('--exec starts with "snap run"; Marathon runs it as a Snap app.');
  if (value.startsWith('/'))
    throw new Error(
      '--exec starts with "/": Marathon keeps only the file name and drops the directory, so the command would not be found. Use a command on PATH, or "sh /absolute/path/to/script".'
    );
  if (value.startsWith('sailfish-qml '))
    throw new Error(
      '--exec starts with "sailfish-qml"; Marathon would run it under the Keel host.'
    );
  const binary = value.split(' ').find((token) => !token.startsWith('-')) ?? '';
  if (X11_ONLY.includes(fileName(binary)))
    throw new Error(`--exec runs ${binary}, an X11-only tool Marathon leaves off the grid.`);
  if (!/^[A-Za-z0-9_./:=+@, -]+$/.test(value))
    throw new Error(
      '--exec contains characters outside letters, digits and _ . / : = + @ , - and space.'
    );
  if (marathonCleanExec(value) !== value)
    throw new Error('--exec would be rewritten by Marathon before it runs.');
  return value;
}

/** @param {unknown} value */
function labelValue(value) {
  if (
    typeof value !== 'string' ||
    !value ||
    value.trim() !== value ||
    /[\\\x00-\x1f\x7f\u2028\u2029]/u.test(value)
  )
    throw new Error(
      'The tile label must be nonempty, without a backslash, control characters or surrounding whitespace.'
    );
  return value;
}

/** The entry for one prepared Marathon app.
 * @param {{appId: string, shortName?: string, displayName: string, platform?: string, icon?: {sizes?: {size: number}[]}}} descriptor
 * @param {{exec?: string, appDir?: string, iconSize?: number}} options */
export function marathonEntry(descriptor, options) {
  const appId = descriptor.appId;
  if (
    typeof appId !== 'string' ||
    appId.endsWith('.desktop') ||
    !/^[A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z][A-Za-z0-9_]*)+$/.test(appId)
  )
    throw new Error('native-app.json has no valid appId; run project prepare-native.');
  const name = labelValue(descriptor.shortName ?? descriptor.displayName);
  const iconSize = options.iconSize ?? MARATHON_ENTRY_ICON_SIZE;
  const sizes = (descriptor.icon?.sizes ?? []).map((entry) => entry.size);
  if (!sizes.includes(iconSize))
    throw new Error(
      `native-app.json has no ${iconSize} px icon. Run project prepare-native with the identity tool.`
    );
  const exec = execValue(options.exec);
  if (typeof options.appDir !== 'string') throw new Error('Pass --app-dir.');
  const appDir = devicePath(options.appDir, '--app-dir');
  const icon = `${appDir}/icons/icon-${iconSize}.png`;
  const desktop = [
    '[Desktop Entry]',
    'Type=Application',
    `Name=${name}`,
    `Exec=${exec}`,
    `Icon=${icon}`,
    'Terminal=false',
    `StartupWMClass=${appId}`,
    'X-Purism-FormFactor=Mobile;',
    '',
  ].join('\n');
  return { fileName: `${appId}.desktop`, desktop, icon, exec, name };
}

const help = `React Native QNX Marathon launcher entry
  project marathon-entry --project <directory> --app-dir <device directory>
                         --exec <command> [--icon-size <px>] [--dry-run]

Reads build/marathon/native-app.json (written by project prepare-native
--platform marathon) and writes build/marathon/desktop/<appId>.desktop, the
freedesktop entry Marathon-Shell lists on its home grid: Name (the tile label),
Exec, Icon, StartupWMClass and X-Purism-FormFactor=Mobile; so the default
"mobile friendly apps" filter keeps it. It prints the file's SHA-256.

--app-dir is the absolute device directory that holds the app's icons/; the
entry uses icons/icon-128.png (--icon-size picks another size of the set).
--exec is the command Marathon starts. Marathon keeps only the file name of a
command that starts with "/", so give a command on PATH or
"sh /absolute/path/to/script". Commands Marathon would rewrite are refused.
Copy the file to ~/.local/share/applications on the device; Marathon rescans
that directory when it changes. The icon files must exist on the device before
the scan.

This is not a signed .marathon package: that manifest takes only a QML entry
point. It does not copy anything to a device, and nothing here has run on a
Marathon device.
`;

/** @param {string[]} argv */
export async function marathonEntryMain(argv) {
  const { values, positionals } = parseArgs({
    args: argv,
    strict: true,
    allowPositionals: true,
    options: {
      help: { type: 'boolean', short: 'h' },
      project: { type: 'string' },
      exec: { type: 'string' },
      'app-dir': { type: 'string' },
      'icon-size': { type: 'string' },
      'dry-run': { type: 'boolean' },
    },
  });
  if (values.help || argv.length === 0) {
    console.log(help);
    return 0;
  }
  if (positionals.length) throw new Error('marathon-entry does not accept positional arguments.');
  if (!values.exec) throw new Error('Pass --exec.');
  if (!values['app-dir']) throw new Error('Pass --app-dir.');
  const iconSize = values['icon-size'] === undefined ? undefined : Number(values['icon-size']);
  if (iconSize !== undefined && !Number.isInteger(iconSize))
    throw new Error('--icon-size must be an integer.');
  const root = path.resolve(values.project ?? '.');
  const descriptorPath = await projectPath(root, 'build/marathon/native-app.json');
  await boundedFile(descriptorPath, 65536);
  const descriptor = JSON.parse(await readFile(descriptorPath, 'utf8'));
  if (descriptor.platform !== 'marathon')
    throw new Error(
      'build/marathon/native-app.json is not a marathon descriptor; run project prepare-native --platform marathon.'
    );
  const entry = marathonEntry(descriptor, {
    exec: values.exec,
    appDir: values['app-dir'],
    iconSize,
  });
  const dryRun = Boolean(values['dry-run']);
  const directory = await projectPath(root, 'build/marathon/desktop', !dryRun);
  if (!dryRun) {
    await mkdir(directory, { recursive: true });
    await writeLeaf(path.join(directory, entry.fileName), entry.desktop, 0o644);
  }
  console.log(
    JSON.stringify(
      {
        action: dryRun ? 'dry-run' : 'written',
        directory,
        file: entry.fileName,
        sha256: createHash('sha256').update(entry.desktop).digest('hex'),
        entry: { name: entry.name, exec: entry.exec, icon: entry.icon },
      },
      null,
      2
    )
  );
  return 0;
}
