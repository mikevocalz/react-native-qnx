// Host-only. The device paths run here against a synthetic ssh and a synthetic app
// log: no board, no QNX. A run on the Pi 400 or the release board is a separate receipt.
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { chmod, cp, mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';

const root = process.cwd();
const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-tree-wait-'));
let cases = 0;
/** @param {boolean} condition @param {string} message */
function check(condition, message) {
  assert.ok(condition, message);
  cases += 1;
}

/** @param {string} script @param {NodeJS.ProcessEnv} [env] */
function runSh(script, env = {}) {
  return spawnSync('sh', ['-c', script], {
    encoding: 'utf8',
    timeout: 30000,
    env: { ...process.env, RNQNX_WAIT_POLL_S: '1', ...env },
  });
}

try {
  // wait-log.sh, run with the same sh the target gets (POSIX, no bashisms).
  const lib = path.join(root, 'scripts/lib/wait-log.sh');
  const log = path.join(temporary, 'app.log');
  await writeFile(log, 'boot\nRNQNX_NODE name=rnqnxTitleBar visible=1\nRNQNX_NODE name=other\n');

  const found = runSh(`. '${lib}'; rnqnx_wait_node '${log}' rnqnxTitleBar 1 5`);
  check(found.status === 0, `node present: ${found.stderr}`);

  // Timeout path: the node never appears. It fails, in about the budget, and says which node.
  const started = Date.now();
  const missing = runSh(`. '${lib}'; rnqnx_wait_node '${log}' rnqnxRenamed 1 2`);
  const waitedMs = Date.now() - started;
  check(missing.status === 124, `timeout status: ${missing.status}`);
  check(/timed out after 2s waiting for node 'rnqnxRenamed'/.test(missing.stderr), missing.stderr);
  check(/0 of 1 lines/.test(missing.stderr), missing.stderr);
  check(waitedMs >= 1500 && waitedMs < 10000, `timeout took ${waitedMs} ms`);

  // A name that is only a prefix of a real node must not match.
  const prefix = runSh(`. '${lib}'; rnqnx_wait_node '${log}' rnqnxTitle 1 1`);
  check(prefix.status === 124, 'prefix of a node name is not a match');

  // The wait ends the moment the line shows up, long before the budget.
  const later = path.join(temporary, 'later.log');
  await writeFile(later, 'boot\n');
  const appears = runSh(
    `. '${lib}'; (sleep 2; echo 'RNQNX_NODE name=late' >> '${later}') & ` +
      `s=$(date +%s); rnqnx_wait_node '${later}' late 1 30; rc=$?; e=$(date +%s); echo "rc=$rc took=$((e - s))"; wait`
  );
  const took = /rc=0 took=(\d+)/.exec(appears.stdout);
  check(took !== null && Number(took[1]) <= 5, `late node: ${appears.stdout}${appears.stderr}`);

  const counts = runSh(`. '${lib}'; rnqnx_wait_log '${log}' '^RNQNX_NODE ' 3 1 'three nodes'`);
  check(counts.status === 124 && /2 of 3 lines/.test(counts.stderr), counts.stderr);
  check(runSh(`. '${lib}'; rnqnx_wait_node '${log}' 'a;b' 1 1`).status === 2, 'bad name refused');
  check(runSh(`. '${lib}'; rnqnx_wait_node '${log}' x 0 1`).status === 2, 'zero count refused');
  check(runSh(`. '${lib}'; rnqnx_wait_log '${log}' x 1 1`).status === 2, 'wrong arity refused');
  const absent = runSh(`. '${lib}'; rnqnx_wait_node '${temporary}/nope.log' x 1 1`);
  check(
    absent.status === 124 && /nope\.log/.test(absent.stderr),
    'missing log times out, names it'
  );

  // The scripts hold no sleep of their own; the waits live in the library.
  for (const script of ['scripts/pi-mouse-smoke.sh', 'scripts/install-pi.sh']) {
    const text = await readFile(path.join(root, script), 'utf8');
    check(!/\bsleep\b/.test(text.replace(/#.*$/gm, '')), `${script} has no sleep`);
  }

  // pi-mouse-smoke.sh against a synthetic target.
  const tree = path.join(temporary, 'tree');
  await mkdir(path.join(tree, 'scripts/lib'), { recursive: true });
  for (const f of ['pi-mouse-smoke.sh', 'install-pi.sh', 'verify-pi-evidence.mjs']) {
    await cp(path.join(root, 'scripts', f), path.join(tree, 'scripts', f));
  }
  for (const f of ['qnx-target.sh', 'qnx-wait.sh', 'wait-log.sh']) {
    await cp(path.join(root, 'scripts/lib', f), path.join(tree, 'scripts/lib', f));
  }
  await writeFile(path.join(tree, 'package.json'), '{"name":"fixture"}');
  const bin = path.join(tree, 'bin');
  await mkdir(bin);
  // ssh: run the remote command locally with sh, stdin passed through.
  await writeFile(path.join(bin, 'ssh'), '#!/bin/sh\nfor last; do :; done\nexec sh -c "$last"\n');
  await writeFile(path.join(bin, 'sudo'), '#!/bin/sh\nshift\nexec "$@"\n');
  // The injector appends what the app would log. FAKE_MODE=drop-frame is the mutation:
  // the title bar node never reacts, so no frame line is logged.
  const inject = path.join(tree, 'fake-inject');
  await writeFile(
    inject,
    `#!${process.execPath}
const fs = require('node:fs');
const log = process.env.FAKE_APP_LOG;
const [verb] = process.argv.slice(2);
const counter = log + '.n';
const n = fs.existsSync(counter) ? Number(fs.readFileSync(counter, 'utf8')) : 0;
if (verb === 'drag') {
  fs.writeFileSync(counter, String(n + 1));
  if (process.env.FAKE_MODE === 'drop-frame') process.exit(0);
  // start 800x600+300+200: the drag moves it by -100,-60, the edge drag trims 100 px
  fs.appendFileSync(log, n === 0 ? 'RNQNX_WINDOW_FRAME 800x600+200+140\\n' : 'RNQNX_WINDOW_FRAME 700x600+200+140\\n');
} else if (verb === 'dclick') {
  fs.appendFileSync(log, 'RNQNX_WINDOW_ACTION maximize\\n');
}
`
  );
  await writeFile(path.join(bin, 'slog2info'), '#!/bin/sh\necho synthetic slog2\n');
  for (const f of [
    path.join(bin, 'ssh'),
    path.join(bin, 'sudo'),
    path.join(bin, 'slog2info'),
    inject,
  ])
    await chmod(f, 0o755);
  const appLog = path.join(tree, 'app.log');
  await writeFile(appLog, 'RNQNX_WINDOW_CHROME platform=qnx\n');
  const env = {
    ...process.env,
    PATH: `${bin}${path.delimiter}${process.env.PATH}`,
    QNX_TARGET_IP: 'fixture.invalid',
    QNX_TARGET_USER: 'qnxuser',
    QNX_TARGET_DIR: tree,
    QNX_SSH_KEY: '',
    QNX_SSH_OPTS: '',
    RNQNX_APP_LOG: appLog,
    RNQNX_INJECT_REMOTE: inject,
    FAKE_APP_LOG: appLog,
    RNQNX_MOUSE_FRAME: '800x600+300+200',
    RNQNX_MOUSE_WAIT_S: '2',
  };
  /** @param {string} name @param {NodeJS.ProcessEnv} extra */
  function mouse(name, extra) {
    return spawnSync('bash', [path.join(tree, 'scripts/pi-mouse-smoke.sh'), '--no-install'], {
      cwd: tree,
      encoding: 'utf8',
      timeout: 60000,
      env: { ...env, RNQNX_MOUSE_EVIDENCE_DIR: path.join(tree, `evidence-${name}`), ...extra },
    });
  }
  const green = mouse('green', {});
  check(green.status === 0, `mouse smoke, app logs frames: ${green.stdout}${green.stderr}`);
  check(/mouse smoke: all checks passed/.test(green.stdout), green.stdout);

  await writeFile(appLog, 'RNQNX_WINDOW_CHROME platform=qnx\n');
  await rm(`${appLog}.n`, { force: true });
  const red = mouse('red', { FAKE_MODE: 'drop-frame' });
  check(red.status !== 0, 'mouse smoke goes red when the frame line never arrives');
  check(/FAIL title-bar drag/.test(red.stdout), red.stdout);
  check(
    /timed out after 2s waiting for a new RNQNX_WINDOW_FRAME line after 'title-bar drag'/.test(
      red.stdout
    ),
    red.stdout
  );

  // install-pi.sh against a synthetic target: the screenshot waits for the first-frame line.
  await writeFile(
    path.join(tree, 'scripts/pi-preflight.sh'),
    `#!/bin/sh
d="$RNQNX_PI_EVIDENCE_DIR"; mkdir -p "$d"
echo gcc_ntoaarch64le > "$d/qcc-V.txt"; echo gcc_ntoaarch64le > "$d/qxx-V.txt"
printf 'RNQNX_PI_PREFLIGHT=1\\nmachine=aarch64le\\nscreen=ready\\ntarget_dir=writable\\n' > "$d/target-preflight.txt"
`
  );
  await writeFile(path.join(tree, 'scripts/deploy-qnx.sh'), '#!/bin/sh\nexit 0\n');
  await writeFile(
    path.join(tree, 'rnqnx_playground'),
    `#!/bin/sh
echo "rnqnx_playground: Screen surface initialized (1280x720)"
[ "$FAKE_MODE" = no-first-frame ] || echo "rnqnx_playground: first frame presented"
echo "rnqnx_playground: exiting cleanly after 600 frames"
`
  );
  await chmod(path.join(tree, 'rnqnx_playground'), 0o755);
  /** @param {NodeJS.ProcessEnv} extra */
  function install(extra) {
    return spawnSync('bash', [path.join(tree, 'scripts/install-pi.sh')], {
      cwd: tree,
      encoding: 'utf8',
      timeout: 60000,
      env: {
        ...env,
        QNX_HOST: '/fixture',
        QNX_TARGET: '/fixture',
        RNQNX_SKIP_BUILD: '1',
        RNQNX_PI_RUNS: '2',
        RNQNX_PI_FRAME_WAIT_S: '2',
        ...extra,
      },
    });
  }
  const installed = install({});
  check(
    installed.status === 0,
    `install-pi, probe prints the first frame: ${installed.stdout}${installed.stderr}`
  );
  const brokenInstall = install({ FAKE_MODE: 'no-first-frame' });
  check(
    brokenInstall.status === 31,
    `install-pi goes red without the line: ${brokenInstall.status}`
  );
  check(
    /timed out after 2s waiting for rnqnx_playground first frame presented \(run 1\)/.test(
      brokenInstall.stderr
    ),
    brokenInstall.stderr
  );
} finally {
  await rm(temporary, { recursive: true, force: true });
}
console.log(`qnx-tree-wait: ${cases} checks passed (host, synthetic target; no board run)`);
