import { TurboModuleRegistry } from 'react-native';
import { scheduleOnUI, scheduleOnRN, getRuntimeKind } from 'react-native-worklets';
function receive(value, kind) {
  globalThis.__recordDelivery();
  globalThis.__workletResult = value;
  globalThis.__workletRuntimeKind = kind;
}
function execute() {
  'worklet';
  scheduleOnRN(receive, 6 * 7, getRuntimeKind());
}
globalThis.__schedulePendingWorklet = () => scheduleOnUI(execute);
scheduleOnUI(execute);

globalThis.__scheduleFailure = () =>
  scheduleOnUI(() => {
    'worklet';
    throw new Error('RNQNX_EXPECTED_WORKLET_FAILURE');
  });

const bootstrap = TurboModuleRegistry.getEnforcing('WorkletsModule');
let rejectedBundleMode = false;
try {
  bootstrap.installTurboModule(true);
} catch (error) {
  rejectedBundleMode = String(error).includes('non-Bundle Mode');
}
globalThis.__rejectedBundleMode = rejectedBundleMode;
function receiveFrame(timestamp) {
  globalThis.__frameTimestampSeen = timestamp;
}
scheduleOnUI(() => {
  'worklet';
  requestAnimationFrame((timestamp) => {
    'worklet';
    scheduleOnRN(receiveFrame, timestamp);
  });
});
