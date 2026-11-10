import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const source = readFileSync(
  new URL('../../packages/react-native-qnx/Libraries/Components/Switch/Switch.js', import.meta.url),
  'utf8'
);

// Retained hook slots and a command/prop-diff native double execute the actual
// override. React scheduling, Fabric command delivery and Qt are separate gates.
/** @param {() => any} getProps */
function mount(getProps) {
  /** @type {any[]} */
  const slots = [];
  /** @type {Array<() => void>} */
  let effects = [];
  let cursor = 0;
  let dirty = true;
  /** @type {any} */
  let rendered;
  /** @type {boolean | undefined} */
  let committedValue;
  /** @type {boolean[]} */
  const commands = [];
  const native = { value: false };
  const forwarded = { current: null };
  const React = {
    /** @param {any} render */
    forwardRef: (render) => render,
    /** @param {any} initial */
    useRef(initial) {
      const index = cursor++;
      if (!slots[index]) slots[index] = { current: initial };
      return slots[index];
    },
    /** @param {any} initial */
    useState(initial) {
      const index = cursor++;
      if (!slots[index]) slots[index] = { value: initial };
      return [
        slots[index].value,
        (/** @type {any} */ value) => {
          slots[index].value = value;
          dirty = true;
        },
      ];
    },
    /** @param {() => void} effect @param {any[]} deps */
    useLayoutEffect(effect, deps) {
      const index = cursor++;
      const previous = slots[index];
      slots[index] = deps;
      if (!previous || deps.some((value, i) => !Object.is(value, previous[i])))
        effects.push(effect);
    },
    /** @param {any} type @param {any} props */
    createElement: (type, props) => ({ type, props }),
  };
  /** @type {{exports: any}} */
  const module = { exports: {} };
  vm.runInNewContext(source, {
    module,
    require: (/** @type {string} */ name) => {
      if (name === 'react') return React;
      if (name.endsWith('/useMergeRefs'))
        return {
          default:
            (/** @type {any} */ local, /** @type {any} */ external) =>
            (/** @type {any} */ item) => {
              local.current = item;
              if (typeof external === 'function') external(item);
              else if (external) external.current = item;
            },
        };
      if (name.endsWith('/SwitchNativeComponent'))
        return {
          default: 'SwitchNativeComponent',
          Commands: {
            setValue: (/** @type {any} */ item, /** @type {boolean} */ value) => {
              assert.equal(item, native);
              commands.push(value);
              item.value = value;
            },
          },
        };
      throw new Error(`Unexpected import ${name}`);
    },
  });
  function flush() {
    let iterations = 0;
    while (dirty) {
      assert.ok(++iterations < 10, 'reconciliation settles without echo loop');
      dirty = false;
      cursor = 0;
      effects = [];
      const element = module.exports(getProps(), forwarded);
      assert.equal(element.type, 'SwitchNativeComponent');
      rendered = element.props;
      rendered.ref(native);
      // Fabric skips identical props; only Commands can repair rejection.
      if (committedValue !== rendered.value) native.value = rendered.value;
      committedValue = rendered.value;
      for (const effect of effects) effect();
    }
  }
  flush();
  return {
    native,
    commands,
    forwarded,
    get props() {
      return rendered;
    },
    update() {
      dirty = true;
      flush();
    },
    toggle() {
      if (rendered.disabled) return;
      native.value = !native.value;
      const event = { nativeEvent: { value: native.value } };
      rendered.onChange(event);
      flush();
      return event;
    },
    detach() {
      rendered.ref(null);
    },
  };
}

// Interpret nested styles using React Native's caller-visible precedence.
// Frozen inputs catch accidental mutation during controlled reconciliation.
/** @param {unknown} style @returns {Record<string, unknown>} */
function flattenStyle(style) {
  if (Array.isArray(style)) return Object.assign({}, ...style.map(flattenStyle));
  return style && typeof style === 'object'
    ? Object.assign({}, /** @type {Record<string, unknown>} */ (style))
    : {};
}
const customStyle = Object.freeze({ alignSelf: 'center', marginLeft: 12 });
const layeredStyle = Object.freeze([
  Object.freeze({ marginLeft: 4, opacity: 0.5 }),
  false,
  Object.freeze([null, customStyle]),
]);
for (const [style, expectedStyle] of [
  [undefined, { alignSelf: 'flex-start' }],
  [customStyle, { alignSelf: 'center', marginLeft: 12 }],
  [layeredStyle, { alignSelf: 'center', marginLeft: 12, opacity: 0.5 }],
]) {
  const styled = mount(() => ({ style }));
  assert.deepEqual(flattenStyle(styled.props.style), expectedStyle);
  assert.equal(styled.forwarded.current, styled.native);
  styled.detach();
  assert.equal(styled.forwarded.current, null);
}

const styledEvents = /** @type {unknown[]} */ ([]);
let styledDisabled = true;
const styled = mount(() => ({
  value: false,
  disabled: styledDisabled,
  style: layeredStyle,
  trackColor: { false: '#112233', true: '#445566' },
  thumbColor: '#ffffff',
  onChange: (/** @type {unknown} */ event) => styledEvents.push(event),
  onValueChange: (/** @type {boolean} */ value) => styledEvents.push(value),
}));
assert.equal(styled.props.disabled, true);
assert.equal(styled.props.value, false);
assert.equal(styled.props.tintColor, '#112233');
assert.equal(styled.props.onTintColor, '#445566');
assert.equal(styled.props.thumbTintColor, '#ffffff');
styled.toggle();
assert.deepEqual(styledEvents, [], 'disabled styled Switch does not emit callbacks');
styledDisabled = false;
styled.update();
const styledEvent = styled.toggle();
assert.deepEqual(styledEvents, [styledEvent, true], 'event identity and callback order survive');
assert.equal(styled.native.value, false, 'styled Switch restores the controlled value');
assert.deepEqual(styled.commands, [false]);
assert.deepEqual(flattenStyle(styled.props.style), {
  alignSelf: 'center',
  marginLeft: 12,
  opacity: 0.5,
});
assert.equal(styled.forwarded.current, styled.native);
styled.detach();
assert.equal(styled.forwarded.current, null);

let rejectedChanges = 0;
let rejectedValues = 0;
const rejected = mount(() => ({
  value: false,
  onChange: () => {
    rejectedChanges += 1;
  },
  onValueChange: (/** @type {boolean} */ value) => {
    assert.equal(value, true);
    rejectedValues += 1;
  },
}));
assert.equal(rejected.forwarded.current, rejected.native, 'native ref is forwarded');
for (let i = 0; i < 4; ++i) {
  rejected.toggle();
  assert.equal(
    rejected.native.value,
    false,
    'same-value parent still restores rejected native proposal'
  );
}
assert.deepEqual(rejected.commands, [false, false, false, false]);
assert.equal(rejectedChanges, 4);
assert.equal(rejectedValues, 4);

let acceptedValue = false;
/** @type {boolean[]} */
const acceptedEvents = [];
const accepted = mount(() => ({
  value: acceptedValue,
  onValueChange: (/** @type {boolean} */ value) => {
    acceptedEvents.push(value);
    acceptedValue = value;
  },
}));
for (let i = 0; i < 20; ++i) accepted.toggle();
assert.equal(accepted.native.value, false);
assert.equal(acceptedEvents.length, 20);
assert.ok(acceptedEvents.every((value, index) => value === (index % 2 === 0)));
assert.deepEqual(accepted.commands, [], 'accepted changes need no rejection command');

let deferredValue = false;
let proposedValue = false;
let disabled = false;
let deferredEvents = 0;
const deferred = mount(() => ({
  value: deferredValue,
  disabled,
  onValueChange: (/** @type {boolean} */ value) => {
    proposedValue = value;
    deferredEvents += 1;
  },
}));
deferred.toggle();
assert.equal(deferred.native.value, false, 'deferred proposal is rejected until parent commits');
deferredValue = proposedValue;
deferred.update();
assert.equal(deferred.native.value, true, 'later parent decision applies');
assert.equal(deferredEvents, 1, 'reconciliation never calls application callbacks');
disabled = true;
deferred.update();
deferred.toggle();
assert.equal(deferred.native.value, true);
assert.equal(deferredEvents, 1);
deferredValue = false;
deferred.update();
assert.equal(deferred.native.value, false, 'external updates apply while disabled');
assert.equal(deferredEvents, 1);
deferred.detach();
assert.equal(deferred.forwarded.current, null, 'unmount clears forwarded ref');
assert.equal(rejected.native.value, false, 'instances remain independent');

console.log(
  'QNX Switch styles/props/events and controlled rejection/acceptance/defer/rapid/disabled/ref behavior passed (hook/native doubles).'
);
