const events = globalThis.wsEvents;
globalThis.wsOpened = events.some((e) => e.name === 'websocketOpen') ? 1 : 0;
globalThis.wsMessages = events.filter((e) => e.name === 'websocketMessage').length;
globalThis.wsEcho = events.some((e) => e.name === 'websocketMessage' && e.data === 'echo:hello from the js thread') ? 1 : 0;
globalThis.wsClosed = events.some((e) => e.name === 'websocketClosed') ? 1 : 0;
globalThis.wsFailed = events.filter((e) => e.name === 'websocketFailed').length;
globalThis.wsAllOnJs = events.every((e) => e.inJs === true) ? 1 : 0;
