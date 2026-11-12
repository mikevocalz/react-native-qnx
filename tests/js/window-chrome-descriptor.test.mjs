import assert from 'node:assert/strict';
import { readFile, readdir } from 'node:fs/promises';
import path from 'node:path';
import {
  readWindowDescriptor,
  windowChromeCMakeFlags,
} from '../../packages/react-native-qnx-cli/src/window-descriptor.mjs';

// The native lane (native/window-chrome) runs the same fixtures through CMake
// and C++. Reading them here keeps the three readers in step.
const fixtures = path.resolve(import.meta.dirname, '../../native/window-chrome/fixtures');
/** @param {string} name */
async function fixture(name) {
  return JSON.parse(await readFile(path.join(fixtures, name), 'utf8'));
}

let count = 0;
/** @param {string} label @param {() => Promise<void> | void} fn */
async function check(label, fn) {
  try {
    await fn();
  } catch (error) {
    throw new Error(`window-chrome-descriptor: ${label}`, { cause: error });
  }
  count++;
}

await check('reads the PR #122 resolved window object', async () => {
  assert.deepEqual(readWindowDescriptor(await fixture('windows-hidden-sized.json')), {
    controls: 'windows',
    titleBarStyle: 'hidden',
    initialSize: { width: 1024, height: 600 },
    legacy: false,
  });
});

await check('fills defaults for an empty window object', async () => {
  assert.deepEqual(readWindowDescriptor(await fixture('empty-window.json')), {
    controls: 'macos',
    titleBarStyle: 'default',
    initialSize: null,
    legacy: false,
  });
});

await check('keeps a descriptor written before the field chromeless', async () => {
  const window = readWindowDescriptor(await fixture('legacy-no-window.json'));
  assert.equal(window.titleBarStyle, 'none');
  assert.equal(window.legacy, true);
});

await check('rejects every invalid fixture the native lane rejects', async () => {
  const invalid = (await readdir(fixtures)).filter((name) => name.startsWith('invalid-'));
  assert.equal(invalid.length, 8);
  for (const name of invalid) {
    const descriptor = await fixture(name);
    assert.throws(() => readWindowDescriptor(descriptor), /native-app\.json window/, name);
  }
});

await check('accepts the maximum edge and rejects one more', () => {
  assert.deepEqual(
    readWindowDescriptor({ window: { initialSize: { width: 16384, height: 1 } } }).initialSize,
    { width: 16384, height: 1 }
  );
  assert.throws(
    () => readWindowDescriptor({ window: { initialSize: { width: 16385, height: 1 } } }),
    /initialSize\.width must be an integer from 1 to 16384/
  );
  assert.throws(
    () => readWindowDescriptor({ window: { initialSize: { width: 100 } } }),
    /must be \{width, height\}/
  );
});

await check('rejects a non-object descriptor', () => {
  assert.throws(() => readWindowDescriptor(null), /must be an object/);
  assert.throws(() => readWindowDescriptor([]), /must be an object/);
});

await check('chrome is opt-in: a descriptor alone adds no CMake flag', async () => {
  const window = readWindowDescriptor(await fixture('windows-hidden-sized.json'));
  assert.deepEqual(windowChromeCMakeFlags(window, false), []);
  assert.deepEqual(windowChromeCMakeFlags(window, true), ['-DRNQNX_ENABLE_WINDOW_CHROME=ON']);
});

await check('--window-chrome with nothing to draw fails instead of building a no-op', async () => {
  const legacy = readWindowDescriptor(await fixture('legacy-no-window.json'));
  assert.throws(() => windowChromeCMakeFlags(legacy, true), /nothing to draw/);
  const none = readWindowDescriptor({ window: { titleBarStyle: 'none' } });
  assert.throws(() => windowChromeCMakeFlags(none, true), /nothing to draw/);
});

console.log(`window-chrome-descriptor: ${count} checks passed`);
