globalThis.voidResolved = 0;
nativeVoid.then(() => (voidResolved = 42));
