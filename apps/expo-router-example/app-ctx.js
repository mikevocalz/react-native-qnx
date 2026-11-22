// Keep non-route components and state outside app/ so Router cannot discover
// them as screens. This matches the QNX adapter's explicit context contract.
export const ctx = require.context('./app', true, /^\.\/(?!.*\+api\.)[^+].*\.[tj]sx?$/, 'sync');
