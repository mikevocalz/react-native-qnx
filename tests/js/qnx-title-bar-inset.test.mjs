import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const base = '../../packages/react-native-qnx/Libraries/Components/SafeAreaView/';
const inset = require(new URL(base + 'TitleBarInset.js', import.meta.url).pathname);

assert.equal(inset.GLOBAL_NAME, '__RNQNX_TITLE_BAR_INSET__');
assert.deepEqual({ ...inset.getTitleBarInset({}) }, { top: 0, right: 0, bottom: 0, left: 0 });
assert.deepEqual(
  { ...inset.getTitleBarInset({ __RNQNX_TITLE_BAR_INSET__: { top: 36, right: 0, bottom: 0, left: 8 } }) },
  { top: 36, right: 0, bottom: 0, left: 8 });
// Hostile or malformed values never reach layout.
assert.deepEqual(
  { ...inset.getTitleBarInset({ __RNQNX_TITLE_BAR_INSET__: { top: -4, right: NaN, bottom: '9', left: Infinity } }) },
  { top: 0, right: 0, bottom: 0, left: 0 });
assert.deepEqual({ ...inset.getTitleBarInset({ __RNQNX_TITLE_BAR_INSET__: 12 }) },
  { top: 0, right: 0, bottom: 0, left: 0 });
assert.deepEqual(inset.titleBarInsetPadding({ top: 36, right: 1, bottom: 2, left: 3 }),
  { paddingTop: 36, paddingRight: 1, paddingBottom: 2, paddingLeft: 3 });

console.log('qnx-title-bar-inset: ok');
