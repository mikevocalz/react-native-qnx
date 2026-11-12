import type { ReactElement, ReactNode } from 'react';
import type { LayoutChangeEvent, StyleProp, ViewStyle } from 'react-native';

/** Props accepted by {@linkcode AdaptiveGrid}. */
export interface AdaptiveGridProps {
  /** Stable-keyed children remain mounted when the number of columns changes. */
  readonly children?: ReactNode;
  /** Preferred minimum item width in logical units. @default 300 */
  readonly minItemWidth?: number;
  /** Horizontal and vertical space between items. @default 16 */
  readonly gap?: number;
  /** Largest useful column count for this content. @default 3 */
  readonly maxColumns?: 1 | 2 | 3;
  /** Outer container styles; padding is excluded from the measured content width. */
  readonly style?: StyleProp<ViewStyle>;
  /** Observes the inner content measurement used by the grid. */
  readonly onLayout?: (event: LayoutChangeEvent) => void;
  /** Identifier of the measured grid container. */
  readonly testID?: string;
}

/** Reflows native View children in the existing Yoga layout tree.
 * Keep stable child keys; grid changes do not remount inputs or regroup children into row parents.
 * @throws RangeError for invalid minimum width, gap, or maximum columns.
 * @example <AdaptiveGrid minItemWidth={320}><Example key="input" /></AdaptiveGrid>
 */
export function AdaptiveGrid(props: AdaptiveGridProps): ReactElement;
