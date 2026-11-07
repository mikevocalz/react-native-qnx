import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import path from 'node:path';
import vm from 'node:vm';

const require = createRequire(import.meta.url);
const rnRoot = path.dirname(require.resolve('react-native/package.json'));
const identities = {
  'Libraries/Renderer/implementations/ReactFabric-dev.js':
    '427afb9b40adb194b0a43064d78b24359c417e1f523cc783d36fe0a0acfa4080',
  'Libraries/Renderer/implementations/ReactFabric-prod.js':
    '8ca1cd03b74d388f1e4400524a1879fe7054da8ff88ae9aef5ac342f07401b36',
  'src/private/renderer/events/dispatchNativeEvent.js':
    '30512f6ea8ff0f8c9becfdcc916b2d5e10e8f82e620a761b2c90e0c0c18d5ac4',
};
for (const [file, expected] of Object.entries(identities)) {
  const source = readFileSync(path.join(rnRoot, file), 'utf8');
  assert.equal(createHash('sha256').update(source).digest('hex'), expected, file);
  if (!file.includes('ReactFabric-')) continue;
  const guards = source.match(
    /\(null != eventTarget \|\|\s*\(globalThis\.__RNQNX_INPUT_LIFECYCLE__ === true &&\s*"topTouchCancel" === topLevelType\)\)/g
  );
  assert.equal(guards?.length, 1, `${file}: unique exact opt-in guard`);
  const guard = guards?.[0];
  assert.ok(guard);
  for (const enabled of [undefined, false, 1, true]) {
    for (const type of ['topTouchStart', 'topTouchEnd', 'topTouchCancel', 'topLayout']) {
      assert.equal(
        vm.runInNewContext(guard, {
          eventTarget: null,
          topLevelType: type,
          globalThis: { __RNQNX_INPUT_LIFECYCLE__: enabled },
        }),
        enabled === true && type === 'topTouchCancel',
        `${file}: ${String(enabled)} ${type}`
      );
    }
    assert.equal(
      vm.runInNewContext(guard, {
        eventTarget: {},
        topLevelType: 'topTouchStart',
        globalThis: { __RNQNX_INPUT_LIFECYCLE__: enabled },
      }),
      true
    );
  }
}

// Execute the installed dispatcher body after stripping Flow/modules. Responder
// and DOM dependencies are controlled doubles here; the native probe exercises
// the real bundled React responder and Pressability separately.
const presetRequire = createRequire(require.resolve('babel-preset-expo'));
const babel = presetRequire('@babel/core');
const source = readFileSync(
  path.join(rnRoot, 'src/private/renderer/events/dispatchNativeEvent.js'),
  'utf8'
);
const transformed = babel.transformSync(source, {
  babelrc: false,
  configFile: false,
  plugins: [
    presetRequire.resolve('@babel/plugin-transform-flow-strip-types'),
    presetRequire.resolve('@babel/plugin-transform-modules-commonjs'),
  ],
});
assert.ok(transformed?.code);
/** @type {{default?: (target: unknown, type: string, payload: unknown) => void}} */
const exported = {};
/** @type {unknown[][]} */
const calls = [];
/** @type {Error | undefined} */
let pendingError;
let rethrows = 0;
vm.runInNewContext(transformed.code, {
  exports: exported,
  require: () => ({
    customBubblingEventTypes: {},
    customDirectEventTypes: {},
    processResponderEvent: (/** @type {unknown[]} */ ...args) => calls.push(args),
    rethrowCaughtError: () => {
      rethrows += 1;
      if (pendingError) throw pendingError;
    },
    dispatchTrustedEvent: () => assert.fail('null cancellation reached DOM dispatch'),
  }),
});
assert.ok(exported.default);
const payload = { changedTouches: [{ identifier: 0, target: 4 }], touches: [] };
exported.default(null, 'topTouchCancel', payload);
assert.deepEqual(calls, [['topTouchCancel', null, payload]]);
assert.equal(rethrows, 1);
pendingError = new Error('null-cancel responder callback failed');
assert.throws(() => exported.default?.(null, 'topTouchCancel', payload), pendingError);
assert.equal(rethrows, 2, 'the null-target early return must execute finally');

const host = readFileSync('vnext/src/runtime/QtReactHost.cpp', 'utf8');
assert.match(
  host,
  /#if defined\(RNQNX_ENABLE_INPUT_LIFECYCLE\) && RNQNX_ENABLE_INPUT_LIFECYCLE[\s\S]*?setProperty\(\*runtime, "__RNQNX_INPUT_LIFECYCLE__", true\);\s*#endif/
);
console.log(
  'Pressable cancellation: exact installed dev/prod guards, OFF parity and null-target error propagation passed'
);
