'use strict';

// SafeAreaView for a window with a client-drawn title bar. Upstream's is a
// View off iOS; this one is a View that also pads for the title bar inset the
// host publishes (TitleBarInset.js), and follows changes to it.

const React = require('react');
const View = require('react-native/Libraries/Components/View/View').default;
const DeviceEventEmitter = require('react-native/Libraries/EventEmitter/RCTDeviceEventEmitter').default;
const {CHANGE_EVENT, getTitleBarInset, titleBarInsetPadding} = require('./TitleBarInset');

function useTitleBarInset() {
  const [inset, setInset] = React.useState(() => getTitleBarInset());
  React.useEffect(() => {
    const subscription = DeviceEventEmitter.addListener(CHANGE_EVENT, () => setInset(getTitleBarInset()));
    // The value may have changed between render and subscription.
    setInset(getTitleBarInset());
    return () => subscription.remove();
  }, []);
  return inset;
}

const SafeAreaView = React.forwardRef(function SafeAreaView({style, ...props}, ref) {
  const inset = useTitleBarInset();
  return React.createElement(View, {...props, ref, style: [style, titleBarInsetPadding(inset)]});
});
SafeAreaView.displayName = 'SafeAreaView';

module.exports = {__esModule: true, default: SafeAreaView, useTitleBarInset};
