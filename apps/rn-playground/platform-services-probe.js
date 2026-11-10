'use strict';

const React = require('react');
const { createStore } = require('zustand/vanilla');
const { useStore } = require('zustand');
const {
  AccessibilityInfo,
  AppRegistry,
  BackHandler,
  Modal,
  Platform,
  View,
  findNodeHandle,
} = require('react-native');
const store = createStore(() => ({
  modal: false,
  target: true,
  disabled: false,
  hidden: false,
  actions: true,
}));
const probe = {
  platform: Platform.OS,
  reactNativeVersion: Platform.constants.reactNativeVersion,
  accessibility: AccessibilityInfo,
  back: BackHandler,
  backEvents: [],
  firstReader: [],
  secondReader: [],
  contrast: [],
  actionEvents: [],
  modalCloses: 0,
  modalShows: 0,
  handled: true,
  setModal: (modal) => store.setState({ modal }),
  setTarget: (target) => store.setState({ target }),
  setDisabled: (disabled) => store.setState({ disabled }),
  setHidden: (hidden) => store.setState({ hidden }),
  setActions: (actions) => store.setState({ actions }),
  installBack() {
    this.firstBack = BackHandler.addEventListener('hardwareBackPress', () => {
      this.backEvents.push('first');
      return false;
    });
    const last = (event) => {
      this.backEvents.push('last');
      this.timestamp = event.timeStamp;
      return this.handled;
    };
    this.lastBack = BackHandler.addEventListener('hardwareBackPress', last);
    // The pinned RN implementation must not duplicate the same callback.
    BackHandler.addEventListener('hardwareBackPress', last);
  },
  installAccessibility() {
    this.first = AccessibilityInfo.addEventListener('screenReaderChanged', (value) =>
      this.firstReader.push(value)
    );
    this.second = AccessibilityInfo.addEventListener('screenReaderChanged', (value) =>
      this.secondReader.push(value)
    );
    this.contrastSub = AccessibilityInfo.addEventListener('highTextContrastChanged', (value) =>
      this.contrast.push(value)
    );
    AccessibilityInfo.isScreenReaderEnabled().then((value) => {
      this.reader = value;
    });
    AccessibilityInfo.isHighTextContrastEnabled().then((value) => {
      this.highContrast = value;
    });
  },
  focus() {
    AccessibilityInfo.sendAccessibilityEvent(this.target, 'focus');
  },
  tag() {
    return findNodeHandle(this.target);
  },
};
globalThis.__rnqnxPlatformServicesProbe = probe;

function Probe() {
  const modal = useStore(store, (state) => state.modal);
  const target = useStore(store, (state) => state.target);
  const disabled = useStore(store, (state) => state.disabled);
  const hidden = useStore(store, (state) => state.hidden);
  const actions = useStore(store, (state) => state.actions);
  return React.createElement(
    View,
    {
      collapsable: false,
      accessible: true,
      role: 'dialog',
      accessibilityViewIsModal: true,
      style: { width: 320, height: 240 },
    },
    target
      ? React.createElement(View, {
          ref: (value) => {
            probe.target = value;
          },
          accessible: true,
          accessibilityLabel: 'Qt focus target',
          accessibilityRole: 'button',
          role: 'combobox',
          accessibilityState: { disabled, expanded: false },
          accessibilityValue: { text: 'Ready' },
          accessibilityElementsHidden: hidden,
          importantForAccessibility: hidden ? 'no-hide-descendants' : 'auto',
          accessibilityActions: actions
            ? ['activate', 'longpress', 'increment', 'decrement'].map((name) => ({ name }))
            : [],
          onAccessibilityAction: (event) => probe.actionEvents.push(event.nativeEvent.actionName),
          collapsable: false,
          style: { width: 100, height: 60 },
        })
      : null,
    modal
      ? React.createElement(
          Modal,
          {
            visible: true,
            onShow: () => {
              probe.modalShows += 1;
            },
            onRequestClose: () => {
              probe.modalCloses += 1;
              store.setState({ modal: false });
            },
          },
          React.createElement(View, { collapsable: false, style: { width: 80, height: 40 } })
        )
      : null
  );
}
AppRegistry.registerComponent('RNQnxPlatformServicesProbe', () => Probe);
