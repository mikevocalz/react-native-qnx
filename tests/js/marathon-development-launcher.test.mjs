import assert from 'node:assert/strict';
import { spawn, spawnSync } from 'node:child_process';
import {
  chmod,
  copyFile,
  mkdir,
  mkdtemp,
  readFile,
  rm,
  symlink,
  writeFile,
} from 'node:fs/promises';
import net from 'node:net';
import os from 'node:os';
import path from 'node:path';

const root = process.cwd();
const cli = path.join(root, 'packages/qnx-devtools/src/marathon.mjs');
const temporary = await mkdtemp(path.join(os.tmpdir(), 'rnqnx-marathon-launch-'));
const runner = path.join(temporary, 'native runner');
const bundle = path.join(temporary, 'app bundle.js');
const runtime = path.join(temporary, 'runtime');
const server = net.createServer();
let checks = 0;
/** @param {string[]} args @param {NodeJS.ProcessEnv} [env] */
function run(args, env = {}) {
  const result = spawnSync(process.execPath, [cli, ...args], {
    cwd: temporary,
    env: { ...process.env, XDG_RUNTIME_DIR: runtime, WAYLAND_DISPLAY: 'wayland-fixture', ...env },
    encoding: 'utf8',
    timeout: 10000,
    killSignal: 'SIGKILL',
    maxBuffer: 1024 * 1024,
  });
  assert.equal(result.error, undefined);
  return result;
}
const base = ['--runner', runner, '--bundle', bundle];
/** @param {string[]} args @param {RegExp} expression @param {NodeJS.ProcessEnv} [env] */
function rejects(args, expression, env) {
  const result = run(args, env);
  assert.notEqual(result.status, 0);
  assert.match(result.stderr, expression);
  checks += 1;
}
try {
  if (process.platform !== 'linux') {
    const unsupported = run(['inspect', ...base]);
    assert.notEqual(unsupported.status, 0);
    assert.match(unsupported.stderr, /requires Linux/);
    console.log(
      'Marathon launcher: non-Linux rejection passed; Linux process/socket suite not run.'
    );
  } else {
    await mkdir(runtime, { mode: 0o700 });
    await writeFile(bundle, 'ok');
    const source = path.join(temporary, 'runner.cpp');
    await writeFile(
      source,
      `#include <fstream>
#include <iostream>
#include <string>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <unistd.h>
int main(int argc, char** argv) {
  if (argc != 3 || std::string(argv[1]) != "--bundle") return 21;
  std::ifstream input(argv[2]); std::string mode; input >> mode;
  auto qpa = std::getenv("QT_QPA_PLATFORM");
  std::cout << "QPA=" << (qpa ? qpa : "unset") << "\\n";
  std::cout << "FD=" << (std::getenv("WAYLAND_SOCKET") ? "present" : "unset") << "\\n";
  std::cout << "PID=" << getpid() << "\\nREADY\\n" << std::flush;
  if (mode == "fail") return 23;
  if (mode == "sleep") std::this_thread::sleep_for(std::chrono::seconds(60));
  return 0;
}
`
    );
    const compiled = spawnSync('c++', [source, '-o', runner], {
      encoding: 'utf8',
      timeout: 30000,
      killSignal: 'SIGKILL',
    });
    assert.equal(compiled.error, undefined);
    assert.equal(compiled.status, 0, compiled.stderr);
    await new Promise((resolve, reject) => {
      server.once('error', reject);
      server.listen(path.join(runtime, 'wayland-fixture'), () => resolve(undefined));
    });

    const inspection = run(['inspect', ...base]);
    assert.equal(inspection.status, 0, inspection.stderr);
    const report = JSON.parse(inspection.stdout);
    assert.equal(report.runner.arch, process.arch);
    assert.equal(report.runner.sharedLibrariesVerified, false);
    assert.equal(report.runtimeVerified, false);
    checks += 1;
    const dry = run(['run', ...base, '--dry-run']);
    assert.equal(dry.status, 0, dry.stderr);
    assert.deepEqual(JSON.parse(dry.stdout).args, ['--bundle', bundle]);
    assert.doesNotMatch(dry.stdout, /READY/);
    checks += 1;
    const launched = run(['run', ...base], { QT_QPA_PLATFORM: 'offscreen', WAYLAND_SOCKET: '77' });
    assert.equal(launched.status, 0, launched.stderr);
    assert.match(launched.stdout, /QPA=wayland/);
    assert.match(launched.stdout, /FD=unset/);
    checks += 1;
    for (const args of [
      ['nope', ...base],
      ['run'],
      ['inspect', ...base, '--dry-run'],
      ['run', ...base, '--timeout', '0'],
      ['run', ...base, '--timeout', '3601'],
      ['run', ...base, '--timeout', '1.5'],
      ['run', ...base, '--runner', runner],
      ['run', ...base, '--extra'],
    ])
      rejects(args, /Use marathon|required|accepts only|timeout|Duplicate|Unknown option/);
    rejects(['run', ...base], /XDG_RUNTIME_DIR/, { XDG_RUNTIME_DIR: '' });
    rejects(['run', ...base], /WAYLAND_DISPLAY/, { WAYLAND_DISPLAY: '../escape' });
    await chmod(runtime, 0o755);
    rejects(['run', ...base], /private directory/);
    await chmod(runtime, 0o700);
    await writeFile(path.join(runtime, 'not-socket'), 'no');
    rejects(['run', ...base], /must be a socket/, { WAYLAND_DISPLAY: 'not-socket' });
    await symlink(runner, path.join(temporary, 'link'));
    rejects(
      ['inspect', '--runner', path.join(temporary, 'link'), '--bundle', bundle],
      /regular file/
    );
    await symlink(bundle, path.join(temporary, 'bundle-link'));
    rejects(
      ['inspect', '--runner', runner, '--bundle', path.join(temporary, 'bundle-link')],
      /regular file/
    );
    const malformed = path.join(temporary, 'malformed');
    await copyFile(runner, malformed);
    const bytes = await readFile(malformed);
    bytes.writeUInt16LE(process.arch === 'x64' ? 183 : 62, 18);
    await writeFile(malformed, bytes);
    rejects(['inspect', '--runner', malformed, '--bundle', bundle], /architecture/);
    await writeFile(malformed, '#!/bin/sh\nexit 0\n');
    rejects(['inspect', '--runner', malformed, '--bundle', bundle], /ELF/);
    await writeFile(bundle, '');
    rejects(['inspect', ...base], /nonempty/);
    await writeFile(bundle, 'fail');
    assert.equal(run(['run', ...base]).status, 23);
    checks += 1;
    await writeFile(bundle, 'sleep');
    const timed = run(['run', ...base, '--timeout', '1']);
    assert.equal(timed.status, 124, timed.stderr);
    checks += 1;
    const pid = Number(timed.stdout.match(/PID=(\d+)/)?.[1]);
    assert.ok(Number.isInteger(pid) && pid > 0);
    assert.throws(() => process.kill(pid, 0), { code: 'ESRCH' });
    checks += 1;

    const child = spawn(process.execPath, [cli, 'run', ...base], {
      cwd: temporary,
      env: { ...process.env, XDG_RUNTIME_DIR: runtime, WAYLAND_DISPLAY: 'wayland-fixture' },
      stdio: ['ignore', 'pipe', 'pipe'],
    });
    const guard = setTimeout(() => child.kill('SIGKILL'), 7000);
    let sent = false;
    let readiness = '';
    child.stdout.on('data', (data) => {
      readiness += data.toString();
      if (!sent && readiness.includes('READY')) {
        sent = true;
        child.kill('SIGTERM');
      }
    });
    const code = await new Promise((resolve, reject) => {
      child.once('error', reject);
      child.once('close', resolve);
    });
    clearTimeout(guard);
    assert.ok(sent);
    assert.equal(code, 143);
    checks += 1;
    console.log(
      `Marathon development launcher: ${checks} checks passed (real local ELF/process/socket; not a compositor or Marathon runtime).`
    );
  }
} finally {
  if (server.listening) await new Promise((resolve) => server.close(resolve));
  await rm(temporary, { recursive: true, force: true });
}
