import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

// Inspected RN 0.88.1 standard config/event sources. A dependency change needs
// renewed API inspection, rather than silently assuming these event semantics.
for (const [file, sha256] of [
  [
    'Libraries/NativeComponent/BaseViewConfig.android.js',
    '7f652698d8805d356fb9c946d0796d3ab3e20902d33c8b241a95d5e23fe25b8b',
  ],
  [
    'ReactCommon/react/renderer/components/view/BaseViewEventEmitter.cpp',
    'cce179fe49fb45ce831ca61deff4dbc97c5ae623406c932f72fe311d261e1f75',
  ],
]) {
  assert.equal(
    createHash('sha256')
      .update(readFileSync(`node_modules/react-native/${file}`))
      .digest('hex'),
    sha256
  );
}

const source = readFileSync(
  'packages/react-native-qnx/Libraries/NativeComponent/BaseViewConfig.js',
  'utf8'
);
const styles = { opacity: true, color: { process: (/** @type {unknown} */ value) => value } };
const context = vm.createContext({
  module: { exports: {} },
  /** @param {string} specifier */
  require(specifier) {
    assert.equal(specifier, 'react-native/Libraries/Components/View/ReactNativeStyleAttributes');
    return { default: styles };
  },
});
vm.runInContext(source, context);
const config = context.module.exports;
for (const prop of [
  'accessible',
  'accessibilityLabel',
  'accessibilityHint',
  'accessibilityRole',
  'accessibilityState',
  'accessibilityActions',
  'onAccessibilityAction',
]) {
  assert.equal(config.validAttributes[prop], true, `Fabric must admit ${prop}`);
}
assert.equal(
  config.directEventTypes.topAccessibilityAction.registrationName,
  'onAccessibilityAction'
);
assert.equal(config.validAttributes.style, styles);
assert.equal(config.validAttributes.onResponderGrant, true);
assert.equal(config.directEventTypes.topLayout.registrationName, 'onLayout');
assert.equal(config.directEventTypes.topAccessibilityTap, undefined);
assert.equal(config.validAttributes.accessibilityValue, undefined);
// Core RN expects the structured values unchanged, rather than a bespoke diff
// or serializer. Native adapters still gate interpretation/delivery separately.
const props = {
  accessible: true,
  accessibilityLabel: 'Synthetic label',
  accessibilityHint: 'Synthetic hint',
  accessibilityRole: 'button',
  accessibilityState: { disabled: true, selected: true, checked: 'mixed' },
  accessibilityActions: [{ name: 'activate', label: 'Synthetic activation' }],
};
const admitted = Object.fromEntries(
  Object.entries(props).filter(([name]) => config.validAttributes[name] === true)
);
assert.deepEqual(admitted, props);
assert.equal(admitted.accessibilityState, props.accessibilityState);
assert.equal(admitted.accessibilityActions, props.accessibilityActions);
console.log('BaseViewConfig admits standard accessibility props and direct action events.');
