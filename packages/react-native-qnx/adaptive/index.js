'use strict';

const { getWindowSizeClass, getGridLayout } = require('./layout');
module.exports = {
  getWindowSizeClass,
  getGridLayout,
  useWindowSizeClass() {
    return getWindowSizeClass(require('react-native').useWindowDimensions());
  },
  get AdaptiveGrid() {
    return require('./AdaptiveGrid').AdaptiveGrid;
  },
  get AdaptiveShell() {
    return require('./AdaptiveShell').AdaptiveShell;
  },
  get defaultAdaptiveColors() {
    return require('./AdaptiveShell').defaultAdaptiveColors;
  },
};
