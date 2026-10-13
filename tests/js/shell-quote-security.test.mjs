import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const fromReactNative = createRequire(require.resolve('react-native/package.json'));
const fromDevtools = createRequire(fromReactNative.resolve('react-devtools-core/package.json'));
const shellQuote = fromDevtools('shell-quote');

// GHSA-pqg4-j6r4-53mv: quoting must reject line terminators after a comment.
// Never execute the returned text; the public quote() contract is the boundary.
for (const terminator of ['\n', '\r', '\u2028', '\u2029']) {
  assert.throws(
    () => shellQuote.quote(['echo', 'ok', { comment: 'note' }, `value${terminator}unexpected`]),
    TypeError
  );
}
const argumentsWithMetacharacters = ['two words', "a'b", '$HOME', 'x;y', 'back\\slash'];
assert.deepEqual(
  shellQuote.parse(shellQuote.quote(argumentsWithMetacharacters)),
  argumentsWithMetacharacters
);
