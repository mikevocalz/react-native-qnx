import { Platform } from 'react-native';

const probe = {
  platform: Platform.OS,
  available: false,
  initial: 'pending',
  repeated: 'pending',
  error: '',
  first: [],
  second: [],
  failures: [],
};
try {
  const { Linking } = require('react-native');
  probe.available = true;
  probe.addThrowingListener = () =>
    Linking.addEventListener('url', () => {
      throw new Error('linking listener failure');
    });
  const first = Linking.addEventListener('url', ({ url }) => probe.first.push(url));
  const second = Linking.addEventListener('url', ({ url }) => probe.second.push(url));
  probe.removeFirst = () => {
    first.remove();
    first.remove();
  };
  probe.removeAll = () => {
    first.remove();
    second.remove();
  };
  probe.clear = () => {
    probe.first.length = 0;
    probe.second.length = 0;
  };
  Linking.getInitialURL().then((value) => {
    probe.initial = value;
  });
  Linking.getInitialURL().then((value) => {
    probe.repeated = value;
  });
  Promise.all(
    [
      Linking.openURL('rnqnx://room/42'),
      Linking.canOpenURL('rnqnx://room/42'),
      Linking.openSettings(),
    ].map((promise) =>
      promise.then(
        () => {
          throw new Error('Unsupported operation succeeded');
        },
        (error) => {
          probe.failures.push(error instanceof Error && /unavailable/.test(error.message));
        }
      )
    )
  ).then(() => {
    probe.failuresSettled = true;
  });
} catch (error) {
  probe.available = false;
  probe.error = String(error);
}
globalThis.__rnqnxLinkingProbe = probe;
