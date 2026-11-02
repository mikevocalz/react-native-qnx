import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

const workspace = await readFile('pnpm-workspace.yaml', 'utf8');
const expected = {
  'source-map-js': '1.2.2',
  '@expo/cli>compression': '1.8.2',
  'react-devtools-core>shell-quote': '1.12.0',
  'nitrogen>zod': '4.6.6',
  'glob>minimatch': '11.0.1',
  glob: '14.0.3',
  'node-forge': '1.4.2',
};
for (const [name, version] of Object.entries(expected)) {
  assert.ok(
    workspace
      .split('\n')
      .some(
        (line) => line.trim() === `'${name}': ${version}` || line.trim() === `${name}: ${version}`
      ),
    `Missing audited security override ${name}@${version}`
  );
}
assert.doesNotMatch(workspace, /GHSA-86w9-cpqp-85rv|audit:\s+ignore:/);
console.log('Security override policy present; registry audit and compat smoke required.');
