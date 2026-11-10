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
  expanded: false,
  value: 'Ready',
}));
const probe = {
  platform: Reflect.get(Platform, 'OS'),
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
  modalWidth: 0,
  modalHeight: 0,
  handled: true,
  setModal: (modal) => store.setState({ modal }),
  setTarget: (target) => store.setState({ target }),
  setDisabled: (disabled) => store.setState({ disabled }),
  setHidden: (hidden) => store.setState({ hidden }),
  setActions: (actions) => store.setState({ actions }),
  updateAccessibleValue: () => store.setState({ expanded: true, value: 'Chosen' }),
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
  tag(name = 'target') {
    return findNodeHandle(this[name]);
  },
};
globalThis.__rnqnxPlatformServicesProbe = probe;

function Probe() {
  const modal = useStore(store, (state) => state.modal);
  const target = useStore(store, (state) => state.target);
  const disabled = useStore(store, (state) => state.disabled);
  const hidden = useStore(store, (state) => state.hidden);
  const actions = useStore(store, (state) => state.actions);
  const expanded = useStore(store, (state) => state.expanded);
  const value = useStore(store, (state) => state.value);
  return React.createElement(
    View,
    {
      ref: (value) => {
        probe.declaration = value;
      },
      collapsable: false,
      accessible: true,
      role: 'dialog',
      accessibilityViewIsModal: true,
      style: {
        position: 'absolute',
        left: 40,
        top: 30,
        width: 180,
        height: 120,
        overflow: 'hidden',
      },
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
          accessibilityState: { disabled, expanded },
          accessibilityValue: { text: value },
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
            ref: (value) => {
              probe.modalHost = value;
            },
            visible: true,
            onShow: () => {
              probe.modalShows += 1;
            },
            onRequestClose: () => {
              probe.modalCloses += 1;
              store.setState({ modal: false });
            },
          },
          React.createElement(View, {
            ref: (value) => {
              probe.modalContent = value;
            },
            collapsable: false,
            style: { flex: 1 },
            onLayout: (event) => {
              probe.modalWidth = event.nativeEvent.layout.width;
              probe.modalHeight = event.nativeEvent.layout.height;
            },
          })
        )
      : null
  );
}
AppRegistry.registerComponent('RNQnxPlatformServicesProbe', () => Probe);
