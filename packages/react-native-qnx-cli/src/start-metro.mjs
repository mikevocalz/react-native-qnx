#!/usr/bin/env node
import { createRequire } from 'node:module';
import path from 'node:path';
import { planProject } from './project.mjs';

// Owned child of project start. Use Metro's public API rather than its CLI:
// the CLI's config-file watcher otherwise survives a rejected port bind.
try {
  const plan = await planProject(['start', ...process.argv.slice(2)]);
  if (!plan || plan.command !== 'start' || plan.dryRun) {
    throw new Error('The Metro server helper requires a validated project start plan.');
  }
  const require = createRequire(path.join(plan.project, 'package.json'));
  const { loadConfig, runServer } = require('metro');
  const config = await loadConfig({
    cwd: plan.project,
    config: plan.config,
    port: plan.port,
    maxWorkers: 2,
    resetCache: plan.resetCache,
  });
  if (config.server?.tls) {
    throw new Error(
      'project start uses loopback HTTP; use your own Metro TLS launcher for a TLS config.'
    );
  }
  await runServer(config, {
    host: '127.0.0.1',
    // This command is an interactive development server, including under CI.
    // Metro otherwise disables file watching when CI=true.
    watch: true,
    onReady: () =>
      console.log(JSON.stringify({ action: 'listening', host: plan.host, port: plan.port })),
    onError: (/** @type {Error} */ error) => {
      console.error(`rnqnx Metro: ${error.message}`);
      process.exit(1);
    },
  });
} catch (error) {
  console.error(`rnqnx Metro: ${error instanceof Error ? error.message : error}`);
  // A failed config/bind can already own workers. The supervising parent kills
  // this process group on exit; do not retain watchers after startup failure.
  process.exit(1);
}
