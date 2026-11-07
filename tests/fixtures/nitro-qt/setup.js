globalThis.calls = 0;
globalThis.microtasks = 0;
globalThis.callback = () => {
  calls++;
  Promise.resolve().then(() => microtasks++);
  return 42;
};
globalThis.fail = () => {
  throw new Error('callback failure');
};
globalThis.failMicrotask = () => {
  queueMicrotask(() => {
    throw new Error('microtask failure');
  });
  return 42;
};
