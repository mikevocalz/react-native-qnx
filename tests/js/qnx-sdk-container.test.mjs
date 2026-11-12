import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { chmod, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';

// Run the real Bash entry point. These inert CLIs model Docker's documented
// DOCKER_CONTEXT precedence and record every requested host/container operation.
// No Docker daemon, Colima VM, licensed SDK or QNX target is contacted.
const directory = await mkdtemp(path.join(os.tmpdir(), 'rnqnx-sdk-container-'));
const stateFile = path.join(directory, 'state.json');
const callsFile = path.join(directory, 'calls.json');
const script = path.resolve('scripts/qnx-sdk-container.sh');
const fixture = `#!/usr/bin/env node
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const stateFile = process.env.RNQNX_SDK_TEST_STATE;
const callsFile = process.env.RNQNX_SDK_TEST_CALLS;
const state = JSON.parse(fs.readFileSync(stateFile, 'utf8'));
const calls = JSON.parse(fs.readFileSync(callsFile, 'utf8'));
const command = path.basename(process.argv[1]);
const args = process.argv.slice(2);
calls.push({ command, args, context: process.env.DOCKER_CONTEXT ?? null,
  host: process.env.DOCKER_HOST ?? null });
fs.writeFileSync(callsFile, JSON.stringify(calls));
if (command === 'colima') {
  if (args[0] === 'list') {
    assert.deepEqual(args, ['list']);
    console.log('PROFILE STATUS ARCH CPUS MEMORY DISK RUNTIME');
    if (state.profileState !== null) console.log(state.profile + ' ' + state.profileState + ' aarch64 2 4GiB 100GiB docker');
  } else {
    assert.deepEqual(args, ['start', '--profile', state.profile]);
    assert.equal(state.profileState, 'Stopped', 'must not create or reconfigure a profile');
    if (state.failColimaStart) process.exit(71);
    state.profileState = 'Running';
    fs.writeFileSync(stateFile, JSON.stringify(state));
  }
} else {
  assert.equal(command, 'docker');
  // A nonempty DOCKER_CONTEXT takes precedence over DOCKER_HOST in Docker.
  if (process.env.DOCKER_CONTEXT) process.exit(73);
  assert.equal(process.env.DOCKER_HOST, 'unix://' + process.env.HOME + '/.colima/' + state.profile + '/docker.sock');
  if (args[0] === 'inspect') {
    assert.deepEqual(args, ['inspect', '--format', '{{.State.Status}}', state.container]);
    if (state.containerState === null) process.exit(1);
    console.log(state.containerState);
  } else if (args[0] === 'start') {
    assert.deepEqual(args, ['start', state.container]);
    assert.ok(['created', 'exited'].includes(state.containerState));
    if (state.failDockerStart) process.exit(72);
    state.containerState = 'running';
    fs.writeFileSync(stateFile, JSON.stringify(state));
  } else {
    assert.deepEqual(args, ['exec', state.container, 'test', '-f', state.sdp]);
    process.exit(state.sdkExists ? 0 : 1);
  }
}
`;

/** @param {Record<string, unknown>} changes @param {string} command */
async function runCase(changes, command = 'status') {
  const state = {
    profile: 'default',
    profileState: 'Running',
    container: 'qnx805-build',
    containerState: 'running',
    sdp: '/work/qnx800/qnxsdp-env.sh',
    sdkExists: true,
    ...changes,
  };
  await writeFile(stateFile, JSON.stringify(state));
  await writeFile(callsFile, '[]');
  const result = spawnSync('bash', [script, command], {
    env: {
      ...process.env,
      PATH: `${directory}${path.delimiter}${process.env.PATH ?? ''}`,
      DOCKER_CONTEXT: 'unrelated-daemon',
      DOCKER_HOST: 'tcp://unrelated.invalid:2375',
      COLIMA_PROFILE: state.profile,
      QNX_SDK_CONTAINER: state.container,
      QNX_SDK_ENV_FILE: state.sdp,
      RNQNX_SDK_TEST_STATE: stateFile,
      RNQNX_SDK_TEST_CALLS: callsFile,
    },
    encoding: 'utf8',
    timeout: 10000,
  });
  assert.ifError(result.error);
  const calls = JSON.parse(await readFile(callsFile, 'utf8'));
  const starts = calls.filter((/** @type {{args: string[]}} */ call) => call.args[0] === 'start');
  return { ...result, calls, starts };
}

try {
  for (const command of ['colima', 'docker']) {
    const file = path.join(directory, command);
    await writeFile(file, fixture);
    await chmod(file, 0o755);
  }
  const originalContext = process.env.DOCKER_CONTEXT;
  const running = await runCase({});
  assert.equal(running.status, 0, running.stdout + running.stderr);
  assert.match(running.stdout, /All checks passed/);
  assert.equal(running.starts.length, 0, 'status must not start anything');
  assert.ok(
    running.calls
      .filter((/** @type {{command: string}} */ c) => c.command === 'docker')
      .every((/** @type {{context: string | null}} */ c) => c.context === null)
  );
  assert.equal(process.env.DOCKER_CONTEXT, originalContext, 'parent context must be unchanged');

  const stoppedStatus = await runCase({ profileState: 'Stopped' });
  assert.equal(stoppedStatus.status, 1);
  assert.equal(stoppedStatus.starts.length, 0);
  for (const changes of [
    { profileState: null },
    { containerState: null },
    { containerState: 'paused' },
  ]) {
    const missing = await runCase(changes, 'start');
    assert.equal(missing.status, 1);
    assert.equal(
      missing.starts.length,
      0,
      'missing/paused resources must not be created or started'
    );
  }

  const resumed = await runCase({ profileState: 'Stopped', containerState: 'exited' }, 'start');
  assert.equal(resumed.status, 0, resumed.stdout + resumed.stderr);
  assert.deepEqual(
    resumed.starts.map((/** @type {{command: string}} */ c) => c.command),
    ['colima', 'docker']
  );
  const created = await runCase({ containerState: 'created' }, 'start');
  assert.equal(created.status, 0);
  assert.equal(created.starts.length, 1);

  assert.equal((await runCase({ sdkExists: false })).status, 1);
  const failedVm = await runCase({ profileState: 'Stopped', failColimaStart: true }, 'start');
  assert.equal(failedVm.status, 71);
  assert.ok(failedVm.calls.every((/** @type {{command: string}} */ c) => c.command !== 'docker'));
  const failedContainer = await runCase(
    { containerState: 'exited', failDockerStart: true },
    'start'
  );
  assert.equal(failedContainer.status, 72);
  assert.ok(
    failedContainer.calls.every((/** @type {{args: string[]}} */ c) => c.args[0] !== 'exec')
  );

  assert.equal(
    (await runCase({ profile: 'qnx-dev', container: 'sdk-dev', sdp: '/sdk path/env.sh' })).status,
    0
  );
  const invalid = await runCase({}, 'remove');
  assert.equal(invalid.status, 2);
  assert.equal(invalid.calls.length, 0);
  console.log('qnx-sdk-container.test.mjs passed (12 inert host-command scenarios; no device run)');
} finally {
  await rm(directory, { recursive: true, force: true });
}
