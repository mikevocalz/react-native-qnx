const log = globalThis.touchLog;
globalThis.touchCount = log.length;
globalThis.touchAfterBlock = log.every((e) => e.at >= globalThis.blockEndedAt) ? 1 : 0;
globalThis.touchStarts = log.filter((e) => e.type === 'topTouchStart').length;
globalThis.touchEnds = log.filter((e) => e.type === 'topTouchEnd').length;
globalThis.touchOrdered = log.length > 0 && log[0].type === 'topTouchStart' ? 1 : 0;
globalThis.touchMoves = log.filter((e) => e.type === 'topTouchMove').length;
