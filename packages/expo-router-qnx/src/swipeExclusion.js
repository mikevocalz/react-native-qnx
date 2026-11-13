// The width, in pixels, of a left-hand tab rail. The drawer's edge swipe zone
// starts to the right of it, so dragging on the rail never opens the drawer.
const listeners = new Set();
let left = 0;

function setSwipeExclusionLeft(value) {
  const next = Math.max(0, Math.round(value) || 0);
  if (next === left) return;
  left = next;
  for (const listener of listeners) listener(left);
}

function getSwipeExclusionLeft() {
  return left;
}

function subscribeSwipeExclusion(listener) {
  listeners.add(listener);
  return () => listeners.delete(listener);
}

module.exports = { setSwipeExclusionLeft, getSwipeExclusionLeft, subscribeSwipeExclusion };
