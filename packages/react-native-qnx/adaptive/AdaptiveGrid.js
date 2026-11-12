'use strict';

const React = require('react');
const { View } = require('react-native');
const { getGridLayout } = require('./layout');
const { useElementSize } = require('./state');
const h = React.createElement;

function AdaptiveGrid({
  children,
  minItemWidth = 300,
  gap = 16,
  maxColumns = 3,
  style,
  onLayout,
  testID,
}) {
  const size = useElementSize();
  const layout = getGridLayout({ width: size.width, minItemWidth, gap, maxColumns });
  const items = React.Children.toArray(children);
  return h(
    View,
    { style: [{ minWidth: 0 }, style] },
    h(
      View,
      {
        testID,
        collapsable: false,
        onLayout: (event) => {
          size.onLayout(event);
          if (onLayout) onLayout(event);
        },
        style: {
          width: '100%',
          minWidth: 0,
          flexDirection: 'row',
          flexWrap: 'wrap',
          alignItems: 'flex-start',
        },
      },
      ...items.map((item, index) =>
        h(
          View,
          {
            // The same child stays below the same keyed wrapper at every column count.
            key: item.key ?? index,
            style: {
              width: size.width > 0 ? layout.itemWidth : '100%',
              minWidth: 0,
              marginRight: index % layout.columns === layout.columns - 1 ? 0 : gap,
              marginBottom: gap,
            },
          },
          item
        )
      )
    )
  );
}

module.exports = { AdaptiveGrid };
