import { execFileSync } from 'node:child_process';

// Exercise the real QNX packaging script with synthetic public-build artifacts.
execFileSync('python3', ['-I', 'tests/python/test_qt_public_staging.py'], {
  cwd: new URL('../..', import.meta.url),
  stdio: 'inherit',
  timeout: 120_000,
});
