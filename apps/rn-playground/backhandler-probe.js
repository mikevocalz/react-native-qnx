import { Platform } from 'react-native';

const probe = { platform: Platform.OS, available: false, error: '', calls: [] };
try {
  const { BackHandler } = require('react-native');
  const availability = BackHandler.addEventListener('hardwareBackPress', () => false);
  availability.remove();
  probe.available = true;
  probe.addThrowingListener = () =>
    BackHandler.addEventListener('hardwareBackPress', () => {
      throw new Error('back listener failure');
    });
  let subscriptions = [];
  probe.configure = (consumeLast) => {
    subscriptions.forEach((subscription) => subscription.remove());
    probe.calls.length = 0;
    subscriptions = [
      BackHandler.addEventListener('hardwareBackPress', () => {
        probe.calls.push('first');
        return false;
      }),
      BackHandler.addEventListener('hardwareBackPress', () => {
        probe.calls.push('last');
        return consumeLast;
      }),
    ];
  };
  probe.removeLast = () => {
    subscriptions[1].remove();
    subscriptions[1].remove();
  };
  probe.removeAll = () => subscriptions.forEach((subscription) => subscription.remove());
  probe.configureDoubleExit = () => {
    probe.removeAll();
    subscriptions = [
      BackHandler.addEventListener('hardwareBackPress', () => {
        BackHandler.exitApp();
        BackHandler.exitApp();
        return false;
      }),
    ];
  };
  probe.exit = () => BackHandler.exitApp();
} catch (error) {
  probe.error = String(error);
}
globalThis.__rnqnxBackHandlerProbe = probe;
