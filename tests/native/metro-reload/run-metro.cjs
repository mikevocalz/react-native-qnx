// Real Metro HTTP -> compiler-enabled Hermes -> Fabric mount/full reload.
// Usage: node tests/native/metro-reload/run-metro.cjs /absolute/path/to/rnqnx_metro_reload_test
const path = require('node:path');
const fs = require('node:fs');
const { createRequire } = require('node:module');
const { spawn } = require('node:child_process');
const root = path.resolve(__dirname, '../../..');
const app = path.join(root, 'apps/rn-playground');
const appRequire = createRequire(path.join(app, 'package.json'));
const { loadConfig, runServer } = appRequire('metro');

async function main() {
  if (!process.argv[2]) throw new Error('Pass the compiled native test executable.');
  const config = await loadConfig({
    cwd: app,
    config: path.join(app, 'metro.public.config.js'),
    port: 0,
    maxWorkers: 2,
  });
  // Support an isolated worktree reusing the installed dependency graph.
  const dependencyRoot = path.dirname(fs.realpathSync(path.join(root, 'node_modules')));
  if (!config.watchFolders.includes(dependencyRoot)) config.watchFolders.push(dependencyRoot);
  let closed;
  const shutdown = new Promise((resolve) => {
    closed = resolve;
  });
  const { httpServer } = await runServer(config, {
    host: '127.0.0.1',
    watch: true,
    onClose: () => closed(),
  });
  let child;
  try {
    const address = httpServer.address();
    if (!address || typeof address === 'string') throw new Error('Metro TCP endpoint unavailable.');
    const url = `http://127.0.0.1:${address.port}/metro-reload-probe.bundle?platform=qnx&dev=true`;
    child = spawn(path.resolve(process.argv[2]), [url], { stdio: 'inherit', env: process.env });
    const deadline = setTimeout(() => child.kill('SIGKILL'), 120000);
    const code = await new Promise((resolve, reject) => {
      child.once('error', reject);
      child.once('exit', (status, signal) => resolve(signal ? 1 : status));
    }).finally(() => clearTimeout(deadline));
    if (code !== 0) throw new Error(`Native Metro test exited ${code}`);
  } finally {
    if (child && child.exitCode === null && !child.killed) child.kill('SIGKILL');
    httpServer.close();
    await shutdown;
  }
}
main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
