// RNQNX_WS_URL and RNQNX_ON_JS are set by the test before this runs.
globalThis.wsEvents = [];
globalThis.__rctDeviceEventEmitter = {
  emit(name, event) {
    globalThis.wsEvents.push({ name, inJs: RNQNX_ON_JS(), data: event.data, code: event.code });
  },
};
const socket = nativeModuleProxy.WebSocketModule;
globalThis.wsSocket = socket;
socket.addListener('websocketOpen');
socket.connect(globalThis.RNQNX_WS_URL, [], {}, 7);
