const t = Date.now();
const ms = globalThis.RNQNX_BLOCK_MS || 5000;
while (Date.now() - t < ms) {}
globalThis.blockEndedAt = Date.now();
