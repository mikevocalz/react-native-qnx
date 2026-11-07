import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import vm from 'node:vm';

const require = createRequire(import.meta.url);
const { createStore } = require('zustand/vanilla');
const source = readFileSync(
  new URL(
    '../../packages/react-native-qnx/Libraries/Components/TextInput/TextInput.js',
    import.meta.url
  ),
  'utf8'
);
// Exercise the component with retained hook slots and the actual vanilla store.
// Native mounting, scheduling and event causality belong to the Qt probe.
/** @type {Array<{current: any}>} */
let slots;
let cursor = 0;
const React = {
  /** @param {Function} render */
  forwardRef: (render) => render,
  /** @param {any} initial */
  useRef: (initial) => {
    const index = cursor++;
    return (slots[index] ??= { current: initial });
  },
  /** @param {any} type @param {any} props */
  createElement: (type, props) => ({ type, props }),
};
const globals = {};
/** @type {{exports: any}} */
const module = { exports: {} };
vm.runInNewContext(source, {
  module,
  globalThis: globals,
  /** @param {string} name */
  require: (name) => {
    if (name === 'react') return React;
    if (name === 'zustand/vanilla') return { createStore };
    /** @param {import('zustand/vanilla').StoreApi<any>} store @param {(state: any) => any} select */
    const useStore = (store, select) => select(store.getState());
    if (name === 'zustand') return { useStore };
    return { default: 'NativeTextInput' };
  },
});
function instance() {
  /** @type {Array<{current: any}>} */
  const refs = [];
  /** @param {any} props @param {any} [ref] */
  return (props, ref) => {
    slots = refs;
    cursor = 0;
    return module.exports(props, ref).props;
  };
}
const first = instance();
const second = instance();
const ref = {};
/** @type {any[]} */
const events = [];
const props = {
  defaultValue: 'A😀é',
  /** @param {any} event */
  onChange: (event) => events.push(event),
  /** @param {string} text */
  onChangeText: (text) => events.push(text),
};
let field = first(props, ref);
assert.equal(field.ref, ref);
assert.equal(field.text.length, 5);
assert.equal(field.mostRecentEventCount, 0);
const event = { nativeEvent: { text: 'A😀éX', eventCount: 1 } };
field.onChange(event);
field = first(props, ref);
assert.equal(field.text, event.nativeEvent.text);
assert.equal(field.mostRecentEventCount, 1);
assert.equal(events[0], event);
assert.equal(events[1], event.nativeEvent.text);
assert.equal(second({ defaultValue: 'independent' }).mostRecentEventCount, 0);
assert.equal(second({ defaultValue: 'ignored after mount' }).text, 'independent');
field = first({ value: 'controlled' });
field.onChange({ nativeEvent: { text: 'native', eventCount: 2 } });
field = first({ value: 'controlled' });
assert.equal(field.text, 'controlled');
assert.equal(field.mostRecentEventCount, 2);
assert.equal(first({}).text, 'A😀éX');
const focusEvent = {};
let observed;
first({
  /** @param {any} value */
  onFocus: (value) => {
    observed = value;
  },
}).onFocus(focusEvent);
assert.equal(observed, focusEvent);
for (const enabled of [undefined, false, 1, true]) {
  globals.__RNQNX_TEXT_INPUT_CONTRACT__ = enabled;
  if (enabled === true) {
    assert.throws(
      () => instance()({ secureTextEntry: true, multiline: true }),
      /RNQNX_TEXT_INPUT_SECURE_MULTILINE_UNSUPPORTED/
    );
    assert.doesNotThrow(() => instance()({ secureTextEntry: true }));
  } else {
    assert.doesNotThrow(() => instance()({ secureTextEntry: true, multiline: true }));
  }
}
console.log(
  'TextInput per-instance state, controlled acknowledgment, ref/event forwarding and secure gate passed.'
);
