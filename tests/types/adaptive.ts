import { createElement } from 'react';
import { Text } from 'react-native';
import {
  AdaptiveGrid,
  AdaptiveShell,
  getGridLayout,
  getWindowSizeClass,
  useWindowSizeClass,
  type GridLayout,
  type WindowSizeClass,
} from '../../packages/react-native-qnx/adaptive';

const grid: GridLayout = getGridLayout({ width: 920, minItemWidth: 300, maxColumns: 2 });
const classes: WindowSizeClass = getWindowSizeClass({ width: 1280, height: 400 });
void grid;
void classes;
// @ts-expect-error Physical-pixel strings are not a logical width.
getWindowSizeClass({ width: '1280px', height: 400 });
// @ts-expect-error The supported grid is deliberately bounded to one, two, or three columns.
getGridLayout({ width: 1280, maxColumns: 4 });

export function AdaptiveTypeExample() {
  const size = useWindowSizeClass();
  return createElement(
    AdaptiveShell,
    {
      title: 'Example',
      navigation: [{ id: 'home', label: 'Home' }],
      selectedId: 'home',
      onSelect() {},
    },
    createElement(
      AdaptiveGrid,
      { maxColumns: 2 },
      createElement(Text, { key: 'classification' }, size.width)
    )
  );
}
