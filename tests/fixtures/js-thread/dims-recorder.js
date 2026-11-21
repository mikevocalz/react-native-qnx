globalThis.dimsEvents = 0;
RN$registerCallableModule('RCTDeviceEventEmitter', () => ({
  emit(name, payload) {
    if (name === 'didUpdateDimensions') {
      globalThis.dimsEvents += 1;
      globalThis.dimsWidth = payload.window.width;
      globalThis.dimsHeight = payload.window.height;
    }
  },
}));
