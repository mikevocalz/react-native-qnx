globalThis.touchLog = [];
globalThis.nativeFabricUIManager.registerEventHandler((instance, type, payload) => {
  if (type.startsWith('topTouch')) {
    globalThis.touchLog.push({ type, at: Date.now(), x: payload.changedTouches[0].pageX });
  }
});
