// Records every AppState change the module delivers, in order. The module is
// created by the first lookup, so this also proves it resolves on the JS thread.
globalThis.appStateSummary = { initial: '', log: '' };
const changes = [];
globalThis.__rctDeviceEventEmitter = {
  emit(name, payload) {
    if (name !== 'appStateDidChange') return;
    changes.push(payload.app_state);
    globalThis.appStateSummary.log = changes.join(',');
  },
};
globalThis.appStateSummary.initial = nativeModuleProxy.AppState.getConstants().initialAppState;
