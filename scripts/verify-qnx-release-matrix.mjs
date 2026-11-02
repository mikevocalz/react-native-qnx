#!/usr/bin/env node
// Validate the repo's publicly sourced QNX 8.0.5 release reference against
// the build support present in git. This is a documentation/source contract
// check; it deliberately does not claim the vendor toolchain is installed.
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

/** @param {string} path */
const read = (path) => readFile(path, 'utf8');
const manifest = JSON.parse(await read('docs/qnx/release-matrix-2026-11-02.json'));
const cmake = await read('CMakeLists.txt');
const readme = await read('README.md');
const pi = await read('scripts/pi-preflight.sh');

assert.equal(manifest.sdp.version, '8.0.5');
assert.equal(manifest.qsti.version, '0.5.0');
assert.equal(manifest.qsti.build, 50);
assert.equal(manifest.bsp.rpi4.build, 670);
assert.equal(manifest.bsp.rpi5.build, 568);
assert.match(cmake, /RNQNX_BUILD_REACT_QNX/);
assert.match(cmake, /find_package\(Qt6 6\.11\.1 REQUIRED COMPONENTS Core Gui Qml Quick\)/);
assert.match(readme, /QNX SDP 8\.0\.5/);
assert.match(readme, /QSTI 0\.5\.0/);
assert.match(pi, /screen|Screen/);
for (const pkg of manifest.packages) {
  assert.match(pkg.package, /^com\.qnx\./);
  assert.ok(pkg.build > 0);
  assert.match(pkg.releaseNotes, /^https:\/\//);
}
if (manifest.currentRepoEvidence.qnxRuntimeDeviceStatus !== 'pending') {
  throw new Error('Do not claim device proof without actual verified QNX evidence');
}
console.log('QNX 8.0.5 release source matrix validated; vendor-installed versions NOT inspected.');
