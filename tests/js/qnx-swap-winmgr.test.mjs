// tools/qnx-winmgr/swap-winmgr.sh on the host with fake processes: a fake
// pidin lists them, fake binaries register themselves. Covers the swap,
// idempotence, --check, --undo and the automatic rollback.
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { chmod, copyFile, mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const script = fileURLToPath(new URL('../../tools/qnx-winmgr/swap-winmgr.sh', import.meta.url));
const dir = await mkdtemp(path.join(tmpdir(), 'rnqnx-swap-'));
/** @type {import('node:child_process').ChildProcess[]} */
const children = [];
const p = (/** @type {string[]} */ ...parts) => path.join(dir, ...parts);

try {
  for (const d of ['state', 'wm', 'bin', 'stage', 'proc', 'desktop']) await mkdir(p(d));
  const procs = p('state/procs');
  const log = p('state/log');
  await writeFile(procs, '');

  /** A fake program: records "<pid> <name>" and which version it is, then waits. */
  const fakeProgram = async (
    /** @type {string} */ file,
    /** @type {string} */ name,
    /** @type {string} */ version,
    extra = ''
  ) => {
    await writeFile(
      file,
      `#!/bin/sh\n${extra}echo "$$ ${name}" >> "${procs}"\necho "${name} ${version}" >> "${log}"\nexec sleep 300\n`
    );
    await chmod(file, 0o755);
  };
  await fakeProgram(p('wm/rnqnx-winmgr'), 'rnqnx-winmgr', 'old');
  await fakeProgram(p('stage/rnqnx-winmgr'), 'rnqnx-winmgr', 'new');
  await fakeProgram(p('bin/demolauncher'), 'demolauncher', 'launcher');
  await fakeProgram(p('bin/fullscreen-winmgr'), 'fullscreen-winmgr', 'stock');
  // The tile launcher: --replace stops the old copy, then starts one.
  await fakeProgram(
    p('bin/run-chrome-app.sh'),
    'org.rnqnx.showcase',
    'tile',
    `for q in $(grep ' org.rnqnx.showcase$' "${procs}" | cut -d' ' -f1); do kill $q 2>/dev/null; done\n`
  );
  await writeFile(
    p('bin/pidin'),
    `#!/bin/sh\necho "     pid name"\nwhile read -r pid name; do kill -0 "$pid" 2>/dev/null && echo "$pid usr/bin/$name"; done < "${procs}"\n`,
    { mode: 0o755 }
  );
  await writeFile(p('io-hid'), '');

  const start = async (/** @type {string} */ file, /** @type {string[]} */ args = []) => {
    const child = spawn(file, args, { stdio: 'ignore' });
    children.push(child);
    await new Promise((r) => setTimeout(r, 300));
    return child;
  };
  const alive = async (/** @type {string} */ name) => {
    const rows = (await readFile(procs, 'utf8')).trim().split('\n').filter(Boolean);
    return rows
      .map((row) => row.split(' '))
      .filter(([pid, n]) => n === name && isAlive(Number(pid)))
      .map(([pid]) => Number(pid));
  };
  const isAlive = (/** @type {number} */ pid) => {
    try {
      process.kill(pid, 0);
      return true;
    } catch {
      return false;
    }
  };

  const env = {
    ...process.env,
    RNQNX_ALLOW_USER: '1',
    RNQNX_WM_STAGED: p('stage/rnqnx-winmgr'),
    RNQNX_WM_DIR: p('wm'),
    RNQNX_WM_LOG: p('state/wm.log'),
    RNQNX_DESKTOP_FILES: p('desktop'),
    RNQNX_TILE_LAUNCHER: p('bin/run-chrome-app.sh'),
    RNQNX_RUN_AS: '',
    RNQNX_IO_HID: p('io-hid'),
    RNQNX_PROC: p('proc'),
    RNQNX_PIDIN: p('bin/pidin'),
    RNQNX_WM_GRACE: '1',
    RNQNX_STOCK_WM: p('bin/fullscreen-winmgr'),
    RNQNX_LAUNCHER: p('bin/demolauncher'),
  };
  const run = (/** @type {string[]} */ args) =>
    /** @type {Promise<{status: number | null, out: string}>} */ (
      new Promise((resolve) => {
        const child = spawn('sh', [script, ...args], { env, stdio: ['ignore', 'pipe', 'pipe'] });
        let out = '';
        child.stdout.on('data', (c) => (out += c));
        child.stderr.on('data', (c) => (out += c));
        child.on('exit', (status) => resolve({ status, out }));
      })
    );

  // The board before: the old manager, the launcher, the tile app, and the
  // watcher subshell rnqnx-desktop.sh left behind.
  const oldWm = await start(p('wm/rnqnx-winmgr'));
  await start(p('bin/demolauncher'));
  await start(p('bin/run-chrome-app.sh'));
  const watcher = await start('sleep', ['300']);
  await mkdir(p('proc', String(watcher.pid)));
  await writeFile(
    p('proc', String(watcher.pid), 'cmdline'),
    'sh\0/system/etc/startup/rnqnx-desktop.sh\0'
  );
  await writeFile(procs, (await readFile(procs, 'utf8')) + `${watcher.pid} sh\n`);
  const oldContent = await readFile(p('wm/rnqnx-winmgr'), 'utf8');

  // --check changes nothing.
  let result = await run(['--check']);
  assert.equal(result.status, 0, result.out);
  assert.match(result.out, /would install/);
  assert.equal(await readFile(p('wm/rnqnx-winmgr'), 'utf8'), oldContent);
  assert.ok(!existsSync(p('wm/rnqnx-winmgr.before-single-instance')));
  assert.equal((await alive('rnqnx-winmgr')).length, 1);

  // The swap.
  result = await run([]);
  assert.equal(result.status, 0, result.out);
  assert.match(result.out, /done: one rnqnx-winmgr/);
  assert.equal(await readFile(p('wm/rnqnx-winmgr.before-single-instance'), 'utf8'), oldContent);
  assert.match(await readFile(p('wm/rnqnx-winmgr'), 'utf8'), /new/);
  assert.equal((await alive('rnqnx-winmgr')).length, 1);
  assert.equal((await alive('fullscreen-winmgr')).length, 0, 'the stock desktop never started');
  assert.equal((await alive('demolauncher')).length, 1);
  assert.equal((await alive('org.rnqnx.showcase')).length, 1);
  await new Promise((r) => setTimeout(r, 200));
  assert.equal(oldWm.signalCode, 'SIGTERM', 'the old manager got SIGTERM, not SIGKILL');
  assert.equal(watcher.signalCode, 'SIGTERM', 'the boot watcher was stopped first');
  assert.ok(/rnqnx-winmgr new/.test(await readFile(log, 'utf8')));
  assert.ok(!/fullscreen-winmgr/.test(await readFile(log, 'utf8')));

  // Again: nothing to do, and the backup keeps the original.
  result = await run([]);
  assert.equal(result.status, 0, result.out);
  assert.match(result.out, /already installed and running/);
  assert.equal(await readFile(p('wm/rnqnx-winmgr.before-single-instance'), 'utf8'), oldContent);

  // --undo puts the old manager back the same way.
  result = await run(['--undo']);
  assert.equal(result.status, 0, result.out);
  assert.equal(await readFile(p('wm/rnqnx-winmgr'), 'utf8'), oldContent);
  assert.equal((await alive('rnqnx-winmgr')).length, 1);
  assert.match(
    (await readFile(log, 'utf8'))
      .trim()
      .split('\n')
      .filter((l) => l.startsWith('rnqnx-winmgr'))
      .at(-1) ?? '',
    /old/
  );

  // A new manager that exits at once: rolled back to the backup, still one manager.
  await writeFile(p('stage/rnqnx-winmgr'), '#!/bin/sh\nexit 1\n', { mode: 0o755 });
  result = await run([]);
  assert.notEqual(result.status, 0);
  assert.match(result.out, /rolling back/);
  assert.equal(await readFile(p('wm/rnqnx-winmgr'), 'utf8'), oldContent);
  assert.equal((await alive('rnqnx-winmgr')).length, 1);
  assert.equal((await alive('demolauncher')).length, 1);

  // Missing staged binary: refuses before touching anything.
  await rm(p('stage/rnqnx-winmgr'));
  result = await run([]);
  assert.notEqual(result.status, 0);
  assert.match(result.out, /missing/);
  assert.equal(await readFile(p('wm/rnqnx-winmgr'), 'utf8'), oldContent);
  assert.equal((await alive('rnqnx-winmgr')).length, 1);
} finally {
  for (const child of children) child.kill('SIGTERM');
  try {
    const rows = (await readFile(p('state/procs'), 'utf8')).trim().split('\n').filter(Boolean);
    for (const row of rows) {
      try {
        process.kill(Number(row.split(' ')[0]), 'SIGTERM');
      } catch {}
    }
  } catch {}
  await new Promise((r) => setTimeout(r, 200));
  await rm(dir, { recursive: true, force: true });
}
console.log('qnx-swap-winmgr: ok');
