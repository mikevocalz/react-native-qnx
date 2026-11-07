globalThis.resolved = 0;
nativePending.then((value) => (resolved = value));
