/** Logical window-size categories returned by {@linkcode getWindowSizeClass}. */
export interface WindowSizeClass {
  /** Available client width: compact <600, medium <840, expanded <1200, large <1600. */
  readonly width: 'compact' | 'medium' | 'expanded' | 'large' | 'extra-large';
  /** Available client height: compact <480, medium <900, expanded >=900. */
  readonly height: 'compact' | 'medium' | 'expanded';
}

/** Classifies available app-window space in logical units, never physical display pixels.
 * @throws RangeError for negative, nonfinite, or nonnumeric dimensions.
 * @example getWindowSizeClass({ width: 900, height: 400 }); // expanded / compact
 */
export function getWindowSizeClass(size: { width: number; height: number }): WindowSizeClass;

/** Observes the same size classes as {@linkcode getWindowSizeClass} through React Native's
 * live useWindowDimensions hook. React owns subscription cleanup when the component unmounts.
 */
export function useWindowSizeClass(): WindowSizeClass;
