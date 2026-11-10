import assert from 'node:assert/strict';
import { spawn, spawnSync } from 'node:child_process';
import { once } from 'node:events';
import { mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { planProject, projectMain } from '../../packages/react-native-qnx-cli/src/project.mjs';

const root = process.cwd();
const scratch = await mkdtemp(path.join(tmpdir(), 'rnqnx-project-start-'));
const cli = path.join(root, 'packages/react-native-qnx-cli/src/bin.mjs');
try {
  await mkdir(path.join(scratch, 'node_modules/metro'), { recursive: true });
  await writeFile(path.join(scratch, 'package.json'), JSON.stringify({ name: 'start-fixture' }));
  await writeFile(
    path.join(scratch, 'index.js'),
    'throw new Error("Do not evaluate during planning");'
  );
  for (const name of ['metro.config.js', 'metro.marathon.config.js']) {
    await writeFile(
      path.join(scratch, name),
      'throw new Error("Do not evaluate during planning");'
    );
  }
  await writeFile(
    path.join(scratch, 'node_modules/metro/package.json'),
    JSON.stringify({
      name: 'metro',
      version: 'fixture-only',
      bin: 'cli.cjs',
      main: 'cli.cjs',
    })
  );
  const metro = path.join(scratch, 'node_modules/metro/cli.cjs');
  await writeFile(metro, 'process.exit(37);');
  const args = ['start', '--project', scratch];
  const plan = await planProject(args);
  assert.ok(plan);
  assert.equal(plan.host, '127.0.0.1');
  assert.equal(plan.port, 8081);
  assert.equal(plan.output, undefined);
  assert.equal(
    plan.bundleUrl,
    'http://127.0.0.1:8081/index.js.bundle?platform=qnx&dev=true&minify=false'
  );
  assert.equal(
    plan.sourceMapUrl,
    'http://127.0.0.1:8081/index.js.map?platform=qnx&dev=true&minify=false'
  );
  assert.equal(plan.fastRefreshVerified, false);
  assert.equal(await projectMain([...args, '--dry-run']), 0);
  await assert.rejects(readFile(path.join(scratch, 'build')), /ENOENT/);
  const marathon = await planProject([
    ...args,
    '--platform',
    'marathon',
    '--port',
    '8099',
    '--reset-cache',
  ]);
  assert.ok(marathon);
  assert.equal(marathon.port, 8099);
  assert.equal(marathon.resetCache, true);
  assert.match(marathon.config, /metro\.marathon\.config\.js$/);
  assert.match(marathon.bundleUrl ?? '', /platform=marathon/);
  for (const extra of [
    ['--out', 'build/app.js'],
    ['--host', '0.0.0.0'],
    ['--port', '0'],
    ['--port', '80'],
    ['--port', '65536'],
    ['--port', '-1'],
    ['--port', '8081x'],
    ['--port', '1.5'],
    ['--port', '8081', '--port', '8082'],
    ['--reset-cache', '--reset-cache'],
  ])
    await assert.rejects(planProject([...args, ...extra]));
  await assert.rejects(planProject(['bundle', '--project', scratch, '--port', '8081']));
  await assert.rejects(planProject(['inspect', '--project', scratch, '--reset-cache']));
  assert.equal(await projectMain(args), 37, 'Metro startup failure must propagate');

  // Verify the installed Metro API receives the validated config without sockets.
  await writeFile(
    metro,
    `exports.loadConfig = async options => {
  require('node:fs').writeFileSync('argv.json', JSON.stringify(options));
  return {server: {port: options.port}};
};
exports.runServer = async (config, options) => {
  require('node:fs').writeFileSync('server.json', JSON.stringify({host: options.host, config}));
  options.onReady();
};`
  );
  const captured = spawnSync(
    process.execPath,
    [cli, 'project', ...args, '--port', '8099', '--reset-cache'],
    {
      encoding: 'utf8',
      timeout: 10000,
    }
  );
  assert.equal(captured.status, 0, captured.stderr);
  const argv = JSON.parse(await readFile(path.join(scratch, 'argv.json'), 'utf8'));
  assert.deepEqual(argv, {
    cwd: scratch,
    config: path.join(scratch, 'metro.config.js'),
    port: 8099,
    maxWorkers: 2,
    resetCache: true,
  });
  assert.deepEqual(JSON.parse(await readFile(path.join(scratch, 'server.json'), 'utf8')), {
    host: '127.0.0.1',
    config: { server: { port: 8099 } },
  });

  // A cooperative child exiting zero on interruption must not turn cancellation
  // into CLI success. No native process or transport is involved in this test.
  await writeFile(
    metro,
    `exports.loadConfig = async () => ({server: {port: 8081}});
exports.runServer = async () => {
  process.on('SIGTERM', () => process.exit(0));
  console.log('SERVER_FIXTURE_READY');
  setInterval(() => {}, 1000);
};`
  );
  const child = spawn(process.execPath, [cli, 'project', ...args], {
    stdio: ['ignore', 'pipe', 'pipe'],
  });
  const closed = once(child, 'close');
  const watchdog = setTimeout(() => child.kill('SIGKILL'), 10000);
  try {
    await new Promise((resolve, reject) => {
      let output = '';
      child.stdout.on('data', (chunk) => {
        output += chunk.toString();
        if (output.includes('SERVER_FIXTURE_READY')) resolve(undefined);
      });
      child.once('error', reject);
      child.once('exit', () => reject(new Error('Fixture exited before ready')));
    });
    child.kill('SIGTERM');
    const [code] = await closed;
    assert.equal(code, 143);
  } finally {
    clearTimeout(watchdog);
    if (child.exitCode === null) child.kill('SIGKILL');
  }
  console.log(
    'External project start: planning, isolation, argv, failure and signal propagation passed (synthetic Metro).'
  );
} finally {
  await rm(scratch, { recursive: true, force: true });
}
