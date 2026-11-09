// Production React Native public APIs, loaded by the native metrics test.
import React from 'react';
import {
  AppRegistry,
  Dimensions,
  PixelRatio,
  Text,
  TextInput,
  useWindowDimensions,
  View,
} from 'react-native';

globalThis.__windowMetricsProbe = {
  initial: Dimensions.get('window'),
  screen: Dimensions.get('screen'),
  firstEvents: 0,
  secondEvents: 0,
};
const probe = globalThis.__windowMetricsProbe;
const first = Dimensions.addEventListener('change', () => probe.firstEvents++);
const second = Dimensions.addEventListener('change', () => {
  probe.secondEvents++;
  probe.latest = Dimensions.get('window');
  probe.screen = Dimensions.get('screen');
});
globalThis.__removeFirstMetricsListener = () => first.remove();
globalThis.__removeSecondMetricsListener = () => second.remove();

function Consumer({ id }) {
  const metrics = useWindowDimensions();
  const [count, setCount] = React.useState(0);
  React.useLayoutEffect(() => {
    probe[id] = {
      ...metrics,
      count,
      pixelRatio: PixelRatio.get(),
      fontRatio: PixelRatio.getFontScale(),
    };
  }, [id, metrics, count]);
  if (id === 'first') globalThis.__bumpMetricsState = () => setCount((value) => value + 1);
  return React.createElement(View, {
    collapsable: false,
    nativeID: id,
    style: { width: metrics.width / 2, height: 30, backgroundColor: '#123456' },
  });
}

function Probe() {
  return React.createElement(
    View,
    { collapsable: false, style: { flex: 1 } },
    React.createElement(Consumer, { id: 'first' }),
    React.createElement(Consumer, { id: 'second' }),
    React.createElement(
      Text,
      {
        nativeID: 'metrics-text',
        style: { fontSize: 15.5, lineHeight: 22.25 },
      },
      'Font scale follows the native layout context.'
    ),
    React.createElement(TextInput, {
      nativeID: 'metrics-empty-input',
      value: '',
      placeholder: 'Empty fields scale too',
      style: { width: 220, height: 44, fontSize: 15.5 },
    })
  );
}

AppRegistry.registerComponent('RNQnxWindowMetrics', () => Probe);
