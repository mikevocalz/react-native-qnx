// Real public refs and Pressability, consumed by the native keyboard fixture.
import React from 'react';
import { AppRegistry, Modal, Pressable, View } from 'react-native';

const h = React.createElement;
globalThis.__viewFocusProbe = { presses: 0, generations: [], events: [], state: {} };
const probe = globalThis.__viewFocusProbe;

function Probe() {
  const [state, setState] = React.useState({
    disabled: false,
    shown: true,
    generation: 0,
    modal: 0,
  });
  const button = React.useRef(null);
  const plain = React.useRef(null);
  const outerSecond = React.useRef(null);
  globalThis.__setFocusProbe = (patch) => setState((previous) => ({ ...previous, ...patch }));
  globalThis.__focusButton = () => button.current?.focus();
  globalThis.__focusPlain = () => plain.current?.focus();
  globalThis.__focusOuterSecond = () => outerSecond.current?.focus();
  const closeModal = (modal) => setState((previous) => ({ ...previous, modal }));
  React.useLayoutEffect(() => {
    probe.state = state;
  }, [state]);
  return h(
    View,
    { collapsable: false, style: { flex: 1 } },
    state.shown &&
      h(Pressable, {
        ref: button,
        testID: 'focus-button',
        role: 'button',
        disabled: state.disabled,
        onFocus: () => probe.events.push('button-focus'),
        onBlur: () => probe.events.push('button-blur'),
        onPress: () => {
          probe.presses++;
          probe.generations.push(state.generation);
        },
        style: { width: 160, height: 48 },
      }),
    h(View, {
      ref: plain,
      testID: 'focus-plain',
      collapsable: false,
      onFocus: () => probe.events.push('plain-focus'),
      onBlur: () => probe.events.push('plain-blur'),
      style: { width: 160, height: 48 },
    }),
    state.modal > 0 &&
      h(
        Modal,
        {
          visible: true,
          transparent: true,
          onRequestClose: () => closeModal(0),
        },
        h(
          View,
          { style: { flex: 1 } },
          h(Pressable, {
            testID: 'outer-first',
            accessibilityRole: 'button',
            style: { width: 160, height: 48 },
          }),
          h(Pressable, {
            ref: outerSecond,
            testID: 'outer-second',
            role: 'button',
            style: { width: 160, height: 48 },
          }),
          state.modal > 1 &&
            h(
              Modal,
              {
                visible: true,
                transparent: true,
                onRequestClose: () => closeModal(1),
              },
              h(
                View,
                { style: { flex: 1 } },
                h(Pressable, {
                  testID: 'inner-first',
                  role: 'button',
                  style: { width: 160, height: 48 },
                }),
                h(Pressable, {
                  testID: 'inner-second',
                  role: 'button',
                  style: { width: 160, height: 48 },
                })
              )
            )
        )
      )
  );
}

AppRegistry.registerComponent('RNQnxViewFocus', () => Probe);
