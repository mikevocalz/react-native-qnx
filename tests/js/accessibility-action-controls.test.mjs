import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { chmodSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const scratch = mkdtempSync(path.join(os.tmpdir(), 'rnqnx-accessibility-controls-'));
const reject = 'RNQNX_ACCESSIBILITY_ACTION_REJECT_OK unsupported-registration';
const missing = 'RNQNX_ACCESSIBILITY_ACTION_FAIL JavaScript accessibility action count mismatch';
try {
  const cases = [
    ['Registration', reject, 0, true],
    ['Registration', reject, 1, false],
    ['Registration', `${reject}\nAddressSanitizer: fixture`, 0, false],
    ['Registration', 'RNQNX_ACCESSIBILITY_ACTION_OK', 0, false],
    ['Negative', missing, 1, true],
    ['Negative', missing, 0, false],
    ['Negative', `${missing}\nruntime error: fixture`, 1, false],
    ['Negative', missing, 139, false],
  ];
  for (const [index, [wrapper, marker, exitCode, accepted]] of cases.entries()) {
    const probe = path.join(scratch, `probe-${index}`);
    // Only fixed test strings enter this synthetic process; no device/app code.
    writeFileSync(probe, `#!/bin/sh\nprintf '%s\\n' '${marker}'\nexit ${exitCode}\n`);
    chmodSync(probe, 0o700);
    const result = spawnSync(
      'cmake',
      [`-DPROBE=${probe}`, '-P', path.resolve(`tests/cmake/AccessibilityAction${wrapper}.cmake`)],
      { encoding: 'utf8', timeout: 5000 }
    );
    assert.equal(result.status === 0, accepted, `${wrapper} case ${index}: ${result.stderr}`);
  }
  console.log(
    'Accessibility action controls enforce exact exit status and reject sanitizer output.'
  );
} finally {
  rmSync(scratch, { recursive: true, force: true });
}
