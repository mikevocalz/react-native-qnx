// rnqnx_native_testing is host test support. No app, runner or QNX target may link it.
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { readFile } from 'node:fs/promises';

const tracked = execFileSync(
  'git',
  ['ls-files', '*CMakeLists.txt', '*.cmake', 'CMakePresets.json'],
  {
    encoding: 'utf8',
  }
)
  .split('\n')
  .filter(Boolean);
assert.ok(tracked.length > 10, 'CMake files found');

let linkers = 0;
for (const file of tracked) {
  const text = await readFile(file, 'utf8');
  if (!text.includes('rnqnx_native_testing')) continue;
  assert.equal(
    file,
    'native/testing/CMakeLists.txt',
    `${file} references rnqnx_native_testing; only native/testing may`
  );
  const links = [
    ...text.matchAll(/target_link_libraries\(\s*(\w+)\s+PRIVATE\s+rnqnx_native_testing/g),
  ];
  for (const [, target] of links) {
    assert.match(
      target,
      /_(test|smoke)$/,
      `${target} links rnqnx_native_testing but is not a test`
    );
    linkers += 1;
  }
}
assert.equal(linkers, 2, 'the node query test and the smoke test are the only linkers');
console.log(
  `native-testing-links: ${linkers} test targets link rnqnx_native_testing, nothing else does`
);
