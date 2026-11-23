'use strict';

const WORKLOADS = Object.freeze([
  {
    id: 'hello-world',
    entry: 'hello-world.js',
    status: 'available-source',
    description: 'The existing minimal View/Text baseline, without a second implementation.',
  },
  {
    id: 'list-1000',
    entry: 'index.js',
    status: 'available-source',
    description: '1,000 fixed-height synthetic rows through the existing FlatList/Qt path.',
  },
  {
    id: 'drawer-drag',
    entry: 'drawer-drag.js',
    config: 'metro.drawer.config.js',
    status: 'available-source',
    description:
      'The shipped JsDrawer (PanResponder edge strip, Animated progress) dragged open and back by pointer events; no script sets its position.',
  },
]);

function createRows(count = 1000) {
  if (!Number.isSafeInteger(count) || count < 1 || count > 1000) {
    throw new RangeError('Performance rows must be an integer from 1 through 1000.');
  }
  return Array.from({ length: count }, (_, index) => ({
    id: 'row-' + String(index + 1).padStart(4, '0'),
    label: 'Synthetic document ' + String(index + 1).padStart(4, '0'),
  }));
}

module.exports = { WORKLOADS, createRows };
