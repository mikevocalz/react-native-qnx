import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdir, mkdtemp, readFile, rm, stat, symlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import {
  desktopEntry,
  desktopEntryMain,
} from '../../packages/react-native-qnx-cli/src/desktop-entry.mjs';

const descriptor = {
  appId: 'org.rnqnx.showcase',
  displayName: 'QNX Components',
  shortName: 'QNX Components',
  icon: { sizes: [16, 24, 32, 48, 64, 96, 128, 192, 256, 512].map((size) => ({ size })) },
};
const options = {
  exec: '/data/home/qnxuser/run-chrome-app.sh',
  appDir: '/data/home/qnxuser/rnqnx-chrome-app-next',
};

const entry = desktopEntry(descriptor, options);
assert.equal(entry.fileName, 'org.rnqnx.showcase.desktop');
// Same keys and order as the stock tiles in /system/etc/desktop_files.
assert.equal(
  entry.desktop,
  [
    '[Desktop Entry]',
    'Name=QNX Components',
    'Exec=/data/home/qnxuser/run-chrome-app.sh',
    'Icon=/data/home/qnxuser/rnqnx-chrome-app-next/icons/icon-128.png',
    'Singleton=Y',
    'Categories=Utilities',
    '',
  ].join('\n')
);
assert.match(entry.launcher, /^\[Launcher\]$/m);
assert.match(entry.launcher, /^DesktopFileName=org\.rnqnx\.showcase\.desktop$/m);
assert.match(entry.launcher, /^Row=1$/m);
assert.match(entry.launcher, /^Order=4$/m);
assert.match(entry.install, /desktop\.conf\.before-rnqnx-tile/);
// Every launcher line is key=value with exactly one '='.
for (const line of entry.launcher
  .split('\n')
  .filter((l) => l && !l.startsWith('#') && !l.startsWith('[')))
  assert.equal(line.split('=').length, 2, line);

// Without --exec the tile goes through the guarded launcher in the app directory,
// and install-tile.sh puts that launcher there.
const guarded = desktopEntry(descriptor, { appDir: options.appDir });
assert.match(guarded.desktop, /^Exec=\/data\/home\/qnxuser\/rnqnx-chrome-app-next\/run-app\.sh$/m);
assert.match(
  guarded.install,
  /cp "\$here\/run-app\.sh" "\/data\/home\/qnxuser\/rnqnx-chrome-app-next\/run-app\.sh"/
);
assert.ok(!entry.install.includes('run-app.sh'), 'a custom --exec script is left alone');
assert.match(guarded.launcherScript, /^APP_ID=org\.rnqnx\.showcase$/m);
assert.ok(!guarded.launcherScript.includes('__APP_ID__'));
assert.equal(spawnSync('sh', ['-n'], { input: guarded.launcherScript }).status, 0);

// The launcher runs as root; --run-as drops to the app's user.
assert.match(
  desktopEntry(descriptor, { ...options, runAs: 'qnxuser' }).desktop,
  /^Exec=\/usr\/bin\/sudo -u qnxuser \/data\/home\/qnxuser\/run-chrome-app\.sh$/m
);
assert.throws(() => desktopEntry(descriptor, { ...options, runAs: 'root; sh' }), /--run-as/);

// The label falls back to the display name.
assert.match(
  desktopEntry({ ...descriptor, shortName: undefined }, options).desktop,
  /^Name=QNX Components$/m
);
// Rejected input.
assert.throws(() => desktopEntry({ ...descriptor, shortName: 'A=B' }, options), /'='/);
assert.throws(() => desktopEntry({ ...descriptor, shortName: 'A\nExec=/bin/sh' }, options));
assert.throws(() => desktopEntry({ ...descriptor, icon: { sizes: [] } }, options), /128 px/);
assert.throws(() => desktopEntry({ ...descriptor, appId: 'nodots' }, options), /appId/);
assert.throws(() => desktopEntry(descriptor, { ...options, exec: 'run.sh' }), /--exec/);
assert.throws(() => desktopEntry(descriptor, { ...options, exec: '/data/a b.sh' }), /--exec/);
assert.throws(() => desktopEntry(descriptor, { ...options, exec: '/data/../etc/x' }), /--exec/);
assert.throws(() => desktopEntry(descriptor, { ...options, appDir: '/data/$HOME' }), /--app-dir/);
assert.throws(() => desktopEntry(descriptor, { ...options, row: 3 }), /--row/);
assert.throws(() => desktopEntry(descriptor, { ...options, order: 0 }), /--order/);

// The command writes the three files under build/qnx/desktop.
const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-desktop-entry-'));
try {
  await mkdir(path.join(temporary, 'build/qnx'), { recursive: true });
  await writeFile(path.join(temporary, 'build/qnx/native-app.json'), JSON.stringify(descriptor));
  const log = console.log;
  console.log = () => {};
  try {
    assert.equal(
      await desktopEntryMain([
        '--project',
        temporary,
        '--exec',
        options.exec,
        '--app-dir',
        options.appDir,
      ]),
      0
    );
  } finally {
    console.log = log;
  }
  const out = path.join(temporary, 'build/qnx/desktop');
  assert.equal(await readFile(path.join(out, 'org.rnqnx.showcase.desktop'), 'utf8'), entry.desktop);
  assert.equal(await readFile(path.join(out, 'launcher-tile.conf'), 'utf8'), entry.launcher);
  assert.equal((await stat(path.join(out, 'install-tile.sh'))).mode & 0o777, 0o755);
  assert.equal((await stat(path.join(out, 'run-app.sh'))).mode & 0o777, 0o755);
  const shell = spawnSync('sh', ['-n', path.join(out, 'install-tile.sh')]);
  assert.equal(shell.status, 0, String(shell.stderr));

  // The install script against a stand-in desktop directory: appends once,
  // keeps one backup.
  const system = path.join(temporary, 'system');
  await mkdir(system);
  await writeFile(path.join(system, 'desktop.conf'), '[Launcher]\nDesktopFileName=gears.desktop\n');
  const script = (await readFile(path.join(out, 'install-tile.sh'), 'utf8')).replace(
    'D=/system/etc/desktop_files',
    `D=${system}`
  );
  await writeFile(path.join(out, 'install-test.sh'), script);
  for (let run = 0; run < 2; run++)
    assert.equal(spawnSync('sh', [path.join(out, 'install-test.sh')]).status, 0);
  const conf = await readFile(path.join(system, 'desktop.conf'), 'utf8');
  assert.equal(conf.split('DesktopFileName=org.rnqnx.showcase.desktop').length, 2);
  assert.equal(
    await readFile(path.join(system, 'desktop.conf.before-rnqnx-tile'), 'utf8'),
    '[Launcher]\nDesktopFileName=gears.desktop\n'
  );
  assert.equal(
    await readFile(path.join(system, 'org.rnqnx.showcase.desktop'), 'utf8'),
    entry.desktop
  );

  // A planted symlink is never written through.
  await rm(path.join(out, 'launcher-tile.conf'));
  await symlink(path.join(temporary, 'elsewhere'), path.join(out, 'launcher-tile.conf'));
  await assert.rejects(
    desktopEntryMain(['--project', temporary, '--exec', options.exec, '--app-dir', options.appDir]),
    /not a regular file/
  );
} finally {
  await rm(temporary, { recursive: true, force: true });
}
console.log('qnx-desktop-entry: ok');
