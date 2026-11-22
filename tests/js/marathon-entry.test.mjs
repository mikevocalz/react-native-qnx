import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { mkdir, mkdtemp, readFile, rm, stat, symlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import {
  MARATHON_ENTRY_ICON_SIZE,
  marathonCleanExec,
  marathonEntry,
  marathonEntryMain,
} from '../../packages/react-native-qnx-cli/src/marathon-entry.mjs';

// Oracle: a second reading of the upstream reader, following
// patrickjquinn/Marathon-Shell@567f1faa crates/marathon-shell-native/src/desktop.rs
// (`read_app`, `fits_a_phone`). The entry is parsed the way the freedesktop
// spec describes. `marathonCleanExec` is the module's own port, so it is not
// independent here; it is held to upstream's unit-test vectors below, and the
// evidence run feeds the generated file to upstream's own Rust code
// (docs/evidence/app-identity-host-vm-2026-11-21/marathon-entry). The last
// value of a repeated key wins in upstream; this oracle refuses repeats.
function parseEntry(text) {
  const groups = new Map();
  let group;
  for (const line of text.split('\n')) {
    if (!line || line.startsWith('#')) continue;
    const header = /^\[([^\]]+)\]$/.exec(line);
    if (header) {
      assert.ok(!groups.has(header[1]), `duplicate group ${header[1]}`);
      groups.set(header[1], new Map());
      group = groups.get(header[1]);
      continue;
    }
    const eq = line.indexOf('=');
    assert.ok(group && eq > 0, `not a key=value line: ${line}`);
    const key = line.slice(0, eq);
    assert.match(key, /^[A-Za-z0-9-]+(\[[^\]]+\])?$/, `bad key ${key}`);
    assert.ok(!group.has(key), `duplicate key ${key}`);
    group.set(key, line.slice(eq + 1));
  }
  return groups;
}

function marathonReadApp(fileName, text) {
  const groups = parseEntry(text);
  const desktop = groups.get('Desktop Entry');
  if (!desktop) return null;
  const get = (key) => desktop.get(key);
  const list = (key) => (get(key) ?? '').split(';').filter(Boolean);
  const yes = (key) => get(key)?.toLowerCase() === 'true';
  if (get('Type') !== undefined && get('Type') !== 'Application') return null;
  if (get('Exec') === undefined) return null;
  const exec = marathonCleanExec(get('Exec'));
  if (exec === null) return null;
  const binary = exec.split(/\s+/).find((token) => !token.startsWith('-')) ?? '';
  if (['xterm', 'xclock', 'xeyes', 'xev', 'xinit', 'xset'].includes(binary.split('/').pop()))
    return null;
  const keel =
    groups.has('X-Sailjail') ||
    get('Exec').startsWith('sailfish-qml ') ||
    yes('X-Marathon-KeelHost');
  if (get('Name') === undefined) return null;
  return {
    id: fileName.replace(/\.desktop$/, ''),
    name: get('Name'),
    icon: get('Icon') ?? '',
    exec,
    keel,
    shown: !yes('NoDisplay') && !yes('Hidden'),
    purism: list('X-Purism-FormFactor'),
    kde: list('X-KDE-FormFactors'),
    categories: list('Categories'),
  };
}

// fits_a_phone with the Settings default filterMobileFriendlyApps = true and
// no Store phone list, which a fresh device does not have.
function fitsAPhone(app) {
  const has = (values, words) =>
    values.some((value) => words.some((word) => value.toLowerCase().includes(word)));
  return (
    has(app.purism, ['mobile']) ||
    has(app.kde, ['handset', 'phone']) ||
    app.categories.some((c) => ['mobile', 'phone'].includes(c.toLowerCase()))
  );
}

const descriptor = {
  appId: 'org.example.fieldnotes',
  displayName: 'Field Notes Professional',
  shortName: 'Notes',
  platform: 'marathon',
  icon: { sizes: [16, 24, 32, 48, 64, 96, 128, 192, 256, 512].map((size) => ({ size })) },
};
const options = { exec: 'sh /home/user/fieldnotes/run.sh', appDir: '/home/user/fieldnotes' };

// The entry, byte for byte. Key order is Type, Name, Exec, Icon, Terminal,
// StartupWMClass, as in the hand-written entries that ran on the VM, then the
// phone-grid hint upstream's filter reads.
const entry = marathonEntry(descriptor, options);
assert.equal(entry.fileName, 'org.example.fieldnotes.desktop');
assert.equal(
  entry.desktop,
  [
    '[Desktop Entry]',
    'Type=Application',
    'Name=Notes',
    'Exec=sh /home/user/fieldnotes/run.sh',
    'Icon=/home/user/fieldnotes/icons/icon-128.png',
    'Terminal=false',
    'StartupWMClass=org.example.fieldnotes',
    'X-Purism-FormFactor=Mobile;',
    '',
  ].join('\n')
);
assert.equal(MARATHON_ENTRY_ICON_SIZE, 128);

// What Marathon's reader makes of it: shown on the phone grid, label is the
// tile label (not expo.name), id is the app id, the command is unchanged, and
// it is not routed through the Keel host.
const app = marathonReadApp(entry.fileName, entry.desktop);
assert.ok(app, 'Marathon would drop the entry');
assert.equal(app.id, 'org.example.fieldnotes');
assert.equal(app.name, 'Notes');
assert.notEqual(app.name, descriptor.displayName);
assert.equal(app.exec, 'sh /home/user/fieldnotes/run.sh');
assert.equal(app.icon, '/home/user/fieldnotes/icons/icon-128.png');
assert.ok(app.shown && !app.keel);
assert.ok(fitsAPhone(app), 'filtered off the grid by the default mobile filter');

// Without the hint the default grid would hide the app: the test is not
// vacuous.
assert.ok(
  !fitsAPhone(marathonReadApp(entry.fileName, entry.desktop.replace(/^X-Purism.*\n/m, ''))),
  'the oracle must reject an entry without the form-factor hint'
);

// No shortName: the label is expo.name, which is what the descriptor stores.
assert.match(
  marathonEntry({ ...descriptor, shortName: undefined }, options).desktop,
  /^Name=Field Notes Professional$/m
);

// Deterministic: the same inputs give the same bytes.
assert.equal(marathonEntry(descriptor, options).desktop, entry.desktop);

// Icon size: another size from the set, nothing else.
assert.match(marathonEntry(descriptor, { ...options, iconSize: 256 }).desktop, /icon-256\.png$/m);
assert.throws(() => marathonEntry(descriptor, { ...options, iconSize: 100 }), /100 px/);
assert.throws(() => marathonEntry({ ...descriptor, icon: { sizes: [] } }, options), /128 px icon/);

// Exec: what Marathon runs must be what we wrote. These are the rewrites of
// clean_exec in desktop.rs, each of which would start something else.
for (const [exec, why] of [
  ['/usr/local/bin/fieldnotes', /directory/],
  ['fieldnotes %U', /field code/],
  ['fieldnotes  --x', /whitespace/],
  ['fieldnotes --new-window', /--new-window/],
  ['flatpak run org.example.App', /flatpak/],
  ['snap run foo', /snap/],
  ['gapplication launch org.example.App', /gapplication/],
  ['fieldnotes --gapplication-service', /gapplication/],
  ['sailfish-qml harbour-foo', /Keel/],
  ['xterm -e top', /X11/],
  ['sh /a/b;rm /c', /characters/],
  ['sh "/a b"', /characters/],
  ['sh /a/b\nExec=evil', /characters/],
  ['', /--exec/],
]) {
  assert.throws(() => marathonEntry(descriptor, { ...options, exec }), why, exec);
}
// A command on PATH, with arguments, passes unchanged.
for (const exec of ['org.example.fieldnotes', 'fieldnotes --verbose --bundle=/a/b.js'])
  assert.equal(marathonCleanExec(exec), exec);
// The port matches the upstream unit tests for the same function.
assert.equal(marathonCleanExec('myapp --verbose %U --file=%f'), 'myapp --verbose --file=');
assert.equal(marathonCleanExec('firefox --new-window %u'), 'firefox');
assert.equal(marathonCleanExec('/usr/bin/my-program --arg'), 'my-program --arg');
assert.equal(marathonCleanExec('gapplication launch org.gnome.Calculator'), 'gnome-calculator');
assert.equal(marathonCleanExec('snap run foo'), 'SNAP:snap run foo');
assert.equal(marathonCleanExec('foo --gapplication-service'), null);
assert.equal(marathonCleanExec('my-new-window-app --some-flag'), 'my-new-window-app --some-flag');
assert.equal(
  marathonCleanExec('/usr/bin/flatpak run --branch=stable org.test.App'),
  'FLATPAK:flatpak run --branch=stable org.test.App'
);

// Label and identity rejects.
for (const shortName of ['A\nExec=/bin/sh', 'back\\slash', ' padded', 'padded ', 'tab\there'])
  assert.throws(
    () => marathonEntry({ ...descriptor, shortName }, options),
    /tile label/,
    shortName
  );
assert.throws(() => marathonEntry({ ...descriptor, appId: 'nodots' }, options), /appId/);
assert.throws(() => marathonEntry({ ...descriptor, appId: 'org.x.desktop' }, options), /appId/);
assert.throws(() => marathonEntry(descriptor, { ...options, appDir: 'relative/dir' }), /--app-dir/);
assert.throws(() => marathonEntry(descriptor, { ...options, appDir: '/a/../b' }), /--app-dir/);
assert.throws(() => marathonEntry(descriptor, { ...options, appDir: '/a b' }), /--app-dir/);
// An absolute icon path: an app dir named like a field code cannot sneak a % in.
assert.throws(() => marathonEntry(descriptor, { ...options, appDir: '/a/%U' }), /--app-dir/);

// A wide label that a user typed into the descriptor is still one line of
// text; the width check belongs to prepare-native, not here.
assert.equal(
  marathonReadApp(
    entry.fileName,
    marathonEntry({ ...descriptor, shortName: '学習ノート' }, options).desktop
  ).name,
  '学習ノート'
);

// The command writes <appId>.desktop under build/marathon/desktop and reports
// its hash.
const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-marathon-entry-'));
try {
  await mkdir(path.join(temporary, 'build/marathon'), { recursive: true });
  await writeFile(
    path.join(temporary, 'build/marathon/native-app.json'),
    JSON.stringify(descriptor)
  );
  const run = async (extra = []) => {
    const lines = [];
    const log = console.log;
    console.log = (line) => lines.push(line);
    try {
      const status = await marathonEntryMain([
        '--project',
        temporary,
        '--exec',
        options.exec,
        '--app-dir',
        options.appDir,
        ...extra,
      ]);
      assert.equal(status, 0);
    } finally {
      console.log = log;
    }
    return JSON.parse(lines.join('\n'));
  };
  const out = path.join(temporary, 'build/marathon/desktop');
  const dry = await run(['--dry-run']);
  assert.equal(dry.action, 'dry-run');
  await assert.rejects(stat(out), /ENOENT/);
  const first = await run();
  assert.equal(first.action, 'written');
  const written = await readFile(path.join(out, 'org.example.fieldnotes.desktop'), 'utf8');
  assert.equal(written, entry.desktop);
  assert.equal((await stat(path.join(out, 'org.example.fieldnotes.desktop'))).mode & 0o777, 0o644);
  assert.equal(first.sha256, createHash('sha256').update(written).digest('hex'));
  assert.deepEqual(first.entry, {
    name: 'Notes',
    exec: 'sh /home/user/fieldnotes/run.sh',
    icon: '/home/user/fieldnotes/icons/icon-128.png',
  });
  // Rerun: same bytes, same hash.
  const second = await run();
  assert.equal(second.sha256, first.sha256);

  // A descriptor for another platform is refused.
  await writeFile(
    path.join(temporary, 'build/marathon/native-app.json'),
    JSON.stringify({ ...descriptor, platform: 'qnx' })
  );
  await assert.rejects(run(), /marathon/);
  await writeFile(
    path.join(temporary, 'build/marathon/native-app.json'),
    JSON.stringify(descriptor)
  );

  // A planted symlink at the leaf is never written through.
  await rm(path.join(out, 'org.example.fieldnotes.desktop'));
  await symlink(
    path.join(temporary, 'elsewhere'),
    path.join(out, 'org.example.fieldnotes.desktop')
  );
  await assert.rejects(run(), /not a regular file/);
  await assert.rejects(stat(path.join(temporary, 'elsewhere')), /ENOENT/);

  // Unknown options and positionals fail.
  await assert.rejects(marathonEntryMain(['--project', temporary, 'stray']), /positional/);
  await assert.rejects(marathonEntryMain(['--project', temporary, '--app-dir', '/a']), /--exec/);
} finally {
  await rm(temporary, { recursive: true, force: true });
}

console.log('marathon-entry tests passed');
