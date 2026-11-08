import { AppState, Platform } from 'react-native';

const first = [];
const second = [];
let firstSubscription;
let secondSubscription;
if (AppState.isAvailable) {
  firstSubscription = AppState.addEventListener('change', (state) => first.push(state));
  secondSubscription = AppState.addEventListener('change', (state) => second.push(state));
}

globalThis.__rnqnxAppStateProbe = {
  available: AppState.isAvailable,
  platform: Platform.OS,
  get current() {
    return AppState.currentState;
  },
  first,
  second,
  clear() {
    first.length = 0;
    second.length = 0;
  },
  removeFirst() {
    firstSubscription?.remove();
  },
  removeAll() {
    firstSubscription?.remove();
    secondSubscription?.remove();
  },
  subscribe() {
    return AppState.addEventListener('change', () => {});
  },
  invalidEvent() {
    return AppState.addEventListener('not-an-event', () => {});
  },
};
console.log(`RNQNX_APPSTATE_PROBE platform=${Platform.OS} available=${AppState.isAvailable}`);
