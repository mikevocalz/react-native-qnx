globalThis.lookups = {};
for (const name of ['AppState', 'WebSocketModule', 'FileReaderModule', 'ImageLoader']) {
  try {
    globalThis.lookups[name] = __turboModuleProxy(name) ? 'ok' : 'null';
  } catch (error) {
    globalThis.lookups[name] = 'threw: ' + error.message;
  }
}
