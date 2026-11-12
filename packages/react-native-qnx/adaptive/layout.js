'use strict';

function length(value, name, allowZero = true) {
  if (
    typeof value !== 'number' ||
    !Number.isFinite(value) ||
    (allowZero ? value < 0 : value <= 0)
  ) {
    throw new RangeError(
      `${name} must be a finite ${allowZero ? 'non-negative' : 'positive'} number of logical units.`
    );
  }
  return value;
}

function getWindowSizeClass({ width, height }) {
  length(width, 'width');
  length(height, 'height');
  return {
    width:
      width < 600
        ? 'compact'
        : width < 840
          ? 'medium'
          : width < 1200
            ? 'expanded'
            : width < 1600
              ? 'large'
              : 'extra-large',
    height: height < 480 ? 'compact' : height < 900 ? 'medium' : 'expanded',
  };
}

function getGridLayout({ width, minItemWidth = 300, gap = 16, maxColumns = 3 }) {
  length(width, 'width');
  length(minItemWidth, 'minItemWidth', false);
  length(gap, 'gap');
  if (![1, 2, 3].includes(maxColumns)) throw new RangeError('maxColumns must be 1, 2, or 3.');
  // Comparing each bounded candidate also avoids overflowing width + gap.
  let columns = 1;
  for (let candidate = 2; candidate <= maxColumns; candidate += 1) {
    if ((width - (candidate - 1) * gap) / candidate >= minItemWidth) columns = candidate;
  }
  return { columns, itemWidth: Math.max(0, (width - gap * (columns - 1)) / columns) };
}

module.exports = { getWindowSizeClass, getGridLayout, length };
