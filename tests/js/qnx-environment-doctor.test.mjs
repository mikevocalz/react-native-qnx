// Prerequisite detection in scripts/doctor.sh (`pnpm qnx doctor`).
// Each case runs the real script with a sandboxed PATH that holds only stub
// tools and the system utilities the script needs, so host-installed CMake,
// Qt or QNX tools cannot leak into the result.
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { chmod, mkdir, mkdtemp, rm, symlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';

const doctor = path.resolve('scripts/doctor.sh');
const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-env-doctor-'));
const which = (/** @type {string} */ name) => {
  const result = spawnSync('/bin/sh', ['-c', `command -v ${name}`], { encoding: 'utf8' });
  return result.status === 0 ? result.stdout.trim() : '';
};
const bash = which('bash');
assert.ok(bash, 'bash is required for the doctor test');
const systemTools = ['date', 'mkdir', 'tee', 'find', 'grep', 'sed', 'head', 'dirname', 'tr', 'cat'];

/** @param {string} dir @param {string} name @param {string} body */
async function stub(dir, name, body) {
  const file = path.join(dir, name);
  await writeFile(file, `#!/bin/sh\n${body}\n`);
  await chmod(file, 0o755);
}

/**
 * @typedef {{os?: string, arch?: string, node?: string|null, pnpm?: string|null,
 *   cmake?: string|null, ninja?: string|null, python?: string|null, qt?: string|null,
 *   qcc?: 'path'|'sdp-only'|'none', sdp?: boolean, screen?: boolean,
 *   manifest?: string|null,
 *   env?: Record<string, string>}} Scenario
 */
let counter = 0;
/** @param {Scenario} scenario */
async function run(scenario) {
  const root = path.join(temporary, String((counter += 1)));
  const bin = path.join(root, 'bin');
  const logs = path.join(root, 'logs');
  await mkdir(bin, { recursive: true });
  for (const tool of systemTools) {
    const real = which(tool);
    assert.ok(real, `${tool} is required for the doctor test`);
    await symlink(real, path.join(bin, tool));
  }
  const os = scenario.os ?? 'Linux';
  const arch = scenario.arch ?? 'x86_64';
  await stub(
    bin,
    'uname',
    `case "$1" in -s) echo ${os};; -m) echo ${arch};; *) echo "${os} 6.0 ${arch}";; esac`
  );
  const versioned = /** @type {const} */ ([
    ['node', scenario.node === undefined ? 'v26.13.0' : scenario.node],
    ['pnpm', scenario.pnpm === undefined ? '12.18.0' : scenario.pnpm],
    ['cmake', scenario.cmake === undefined ? 'cmake version 4.1.2' : scenario.cmake],
    ['ninja', scenario.ninja === undefined ? '1.13.1' : scenario.ninja],
    ['python3', scenario.python === undefined ? 'Python 3.13.1' : scenario.python],
  ]);
  for (const [name, version] of versioned) {
    // A stub node also stands in for scripts/validate-dependencies.mjs and passes.
    if (version !== null)
      await stub(bin, name, `[ "$1" = --version ] && echo "${version}"; exit 0`);
  }
  const qt = scenario.qt === undefined ? '6.12.0' : scenario.qt;
  if (qt !== null) await stub(bin, 'qtpaths6', `[ "$1" = --qt-version ] && echo ${qt}; exit 0`);
  // Fixture checkout: the doctor never reads this repository's own install,
  // so the test passes in a fresh clone before pnpm install.
  const project = path.join(root, 'project');
  for (const dir of ['native', 'packages', 'apps', 'scripts', 'tests', 'cmake', 'node_modules'])
    await mkdir(path.join(project, dir), { recursive: true });
  await mkdir(path.join(project, 'docs/getting-started'), { recursive: true });
  await writeFile(path.join(project, 'docs/getting-started/compatibility-matrix.md'), '');
  const manifest =
    scenario.manifest === undefined
      ? '{\n  "name": "react-native-qnx-monorepo",\n  "packageManager": "pnpm@12.18.0"\n}\n'
      : scenario.manifest;
  if (manifest !== null) await writeFile(path.join(project, 'package.json'), manifest);
  /** @type {Record<string, string>} */
  const env = {
    PATH: bin,
    HOME: root,
    RNQNX_DOCTOR_LOG_DIR: logs,
    RNQNX_DOCTOR_PROJECT_ROOT: project,
  };
  if (scenario.sdp ?? true) {
    const host = path.join(root, 'sdp/host');
    const target = path.join(root, 'sdp/target');
    await mkdir(path.join(host, 'usr/bin'), { recursive: true });
    await mkdir(target, { recursive: true });
    const qcc = scenario.qcc ?? 'path';
    if (qcc !== 'none') {
      for (const tool of ['qcc', 'q++']) {
        await stub(
          path.join(host, 'usr/bin'),
          tool,
          'echo "cc: targets available: gcc_ntoaarch64le"'
        );
        if (qcc === 'path') await symlink(path.join(host, 'usr/bin', tool), path.join(bin, tool));
      }
    }
    if (scenario.screen ?? true) {
      await mkdir(path.join(target, 'usr/include/screen'), { recursive: true });
      await mkdir(path.join(target, 'usr/include/EGL'), { recursive: true });
      await writeFile(path.join(target, 'usr/include/screen/screen.h'), '');
      await writeFile(path.join(target, 'usr/include/EGL/egl.h'), '');
      const lib = path.join(target, 'aarch64le/usr/lib');
      await mkdir(lib, { recursive: true });
      await writeFile(path.join(lib, 'libscreen.so.1'), '');
      // The SDP ships libscreen.so as a symlink; it must count as present.
      await symlink('libscreen.so.1', path.join(lib, 'libscreen.so'));
    }
    env.QNX_HOST = host;
    env.QNX_TARGET = target;
  }
  Object.assign(env, scenario.env ?? {});
  const result = spawnSync(bash, [doctor], { env, encoding: 'utf8', timeout: 30000 });
  assert.equal(result.error, undefined);
  return { status: result.status, out: result.stdout + result.stderr };
}

let checks = 0;
try {
  // Fully provisioned Linux host without a board: passes, target is skipped.
  let r = await run({});
  assert.equal(r.status, 0, r.out);
  assert.match(r.out, /OK: QNX Screen library present .*libscreen\.so/);
  assert.match(r.out, /OK: Qt 6\.12\.0 host tools found/);
  assert.match(r.out, /SKIPPED: target device \(no target configured;/);
  assert.match(r.out, /Next step: pnpm qnx configure-qnx --arch aarch64 --config Debug/);
  assert.doesNotMatch(r.out, /WARNING/);
  checks += 1;

  // SDP not sourced: one root-cause error, dependent checks skipped.
  r = await run({ sdp: false });
  assert.equal(r.status, 1);
  assert.equal(r.out.match(/ERROR/g)?.length, 1, r.out);
  assert.match(r.out, /ERROR: The QNX SDP environment is not loaded/);
  assert.match(r.out, /Fix: source <qnx-installation>\/qnxsdp-env\.sh/);
  assert.match(r.out, /SKIPPED: qcc\/q\+\+ \(QNX SDP environment is not loaded\)/);
  assert.match(r.out, /Start with: source <qnx-installation>\/qnxsdp-env\.sh/);
  checks += 1;

  // SDP sourced on macOS: SDP 8.0 has no macOS host tools.
  r = await run({ os: 'Darwin', arch: 'arm64', qcc: 'none' });
  assert.equal(r.status, 1);
  assert.match(r.out, /ERROR: qcc is not available for Darwin/);
  assert.match(r.out, /Fix: run QNX configure\/build on x86-64 Ubuntu 22\.04/);
  assert.match(r.out, /WARNING: Host OS is Darwin/);
  checks += 1;

  // qcc installed but its directory is not on PATH.
  r = await run({ qcc: 'sdp-only' });
  assert.equal(r.status, 1);
  assert.match(
    r.out,
    /ERROR: qcc exists in \$\{QNX_HOST\}\/usr\/bin but that directory is not on PATH/
  );
  assert.match(r.out, /Fix: export PATH="\$\{QNX_HOST\}\/usr\/bin:\$\{PATH\}"/);
  checks += 1;

  // qcc missing from a Linux SDP install.
  r = await run({ qcc: 'none' });
  assert.match(r.out, /ERROR: qcc is missing from \$\{QNX_HOST\}\/usr\/bin/);
  assert.match(r.out, /Fix: repair or reinstall QNX SDP 8\.0\.5/);
  checks += 1;

  // Screen development files missing.
  r = await run({ screen: false });
  assert.equal(r.status, 1);
  assert.match(r.out, /ERROR: QNX Screen headers missing/);
  assert.match(r.out, /Fix: install the Screen\/graphics development package/);
  assert.match(r.out, /WARNING: EGL headers not found/);
  checks += 1;

  // Node missing and too old.
  r = await run({ node: null });
  assert.equal(r.status, 1);
  assert.match(
    r.out,
    /ERROR: node is not installed or not on PATH\.\n {4}Fix: install Node\.js 26/
  );
  r = await run({ node: 'v24.1.0' });
  assert.match(r.out, /ERROR: node 24\.1\.0 is too old; this repository needs >=26\.0\.0\./);
  checks += 1;

  // pnpm missing or the wrong major (Homebrew pnpm 10 cannot self-switch to 12).
  r = await run({ pnpm: null });
  assert.match(
    r.out,
    /ERROR: pnpm is not installed or not on PATH\.\n {4}Fix: install pnpm 12\.18\.0/
  );
  r = await run({ pnpm: '10.32.1' });
  assert.equal(r.status, 1);
  assert.match(r.out, /ERROR: pnpm reported '10\.32\.1', but this repository pins 12\.18\.0\./);
  assert.match(r.out, /npm install -g pnpm@12\.18\.0/);
  checks += 1;

  // CMake, Ninja and Python minimums name the bootstrap script for the host.
  r = await run({ cmake: 'cmake version 3.22.1' });
  assert.match(
    r.out,
    /ERROR: cmake 3\.22\.1 is too old.*\n {4}Fix: .*\.\/scripts\/bootstrap-host\.sh/
  );
  r = await run({ os: 'Darwin', qcc: 'path', ninja: null });
  assert.match(
    r.out,
    /ERROR: ninja is not installed.*\n {4}Fix: .*\.\/scripts\/bootstrap-macos\.sh/
  );
  r = await run({ python: 'Python 3.9.6' });
  assert.match(r.out, /ERROR: python3 3\.9\.6 is too old/);
  checks += 1;

  // Qt host tools are a warning: the Screen probe builds without them.
  r = await run({ qt: null });
  assert.equal(r.status, 0, r.out);
  assert.match(r.out, /WARNING: Qt host tools \(qtpaths\/qmake\) not found/);
  assert.match(r.out, /Fix: install Qt 6\.11\.1 or newer host tools/);
  r = await run({ qt: '6.8.3' });
  assert.match(r.out, /WARNING: Qt 6\.8\.3 host tools are older than 6\.11\.1\./);
  checks += 1;

  // Half-configured target.
  r = await run({ env: { QNX_TARGET_IP: 'qnxpi.local' } });
  assert.equal(r.status, 0, r.out);
  assert.match(r.out, /OK: QNX_TARGET_IP=qnxpi\.local/);
  assert.match(r.out, /WARNING: QNX_TARGET_IP is set but QNX_TARGET_USER is not/);
  r = await run({ env: { QNX_TARGET_IP: 'qnxpi.local', QNX_TARGET_USER: 'qnxuser' } });
  assert.doesNotMatch(r.out, /WARNING|SKIPPED: target/);
  checks += 1;

  // The pnpm pin comes from packageManager; an integrity suffix is ignored.
  r = await run({
    manifest:
      '{"name": "react-native-qnx-monorepo", "packageManager": "pnpm@12.18.0+sha512.abc123"}\n',
  });
  assert.equal(r.status, 0, r.out);
  assert.match(r.out, /OK: pnpm 12\.18\.0 matches the repository pin/);
  r = await run({ manifest: '{"name": "x", "packageManager": "pnpm@12.20.1"}\n' });
  assert.match(r.out, /ERROR: pnpm reported '12\.18\.0', but this repository pins 12\.20\.1\./);
  checks += 1;

  // A missing package.json is reported, not a silent abort before the summary.
  r = await run({ manifest: null });
  assert.equal(r.status, 1);
  assert.match(r.out, /ERROR: package\.json not found at project root/);
  assert.match(r.out, /Environment has \d+ error\(s\)/);
  checks += 1;

  // Every ERROR or WARNING carries a Fix line.
  for (const scenario of [{ sdp: false, node: null, pnpm: null, qt: null }, { screen: false }]) {
    r = await run(scenario);
    const lines = r.out.split('\n');
    lines.forEach((line, index) => {
      if (/^\s+(ERROR|WARNING):/.test(line))
        assert.match(lines[index + 1] ?? '', /^ {4}Fix: \S/, `no fix after: ${line}`);
    });
  }
  checks += 1;
} finally {
  await rm(temporary, { recursive: true, force: true });
}
console.log(`qnx-environment-doctor: ${checks} prerequisite checks passed`);
