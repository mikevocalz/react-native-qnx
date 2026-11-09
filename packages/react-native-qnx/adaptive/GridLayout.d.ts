/** Sizing inputs for {@linkcode getGridLayout}; all lengths use logical units. */
export interface GridLayoutOptions {
  /** Measured content width after its parent padding and navigation have been removed. */
  readonly width: number;
  /** Preferred minimum item width. A narrower container still shows one fitting column. @default 300 */
  readonly minItemWidth?: number;
  /** Space between columns, in logical units. @default 16 */
  readonly gap?: number;
  /** Maximum number of columns. @default 3 */
  readonly maxColumns?: 1 | 2 | 3;
}

/** Resolved column sizing from {@linkcode getGridLayout}. */
export interface GridLayout {
  /** Number of columns that fit; one when width is zero during initial layout. */
  readonly columns: 1 | 2 | 3;
  /** Width of each item after subtracting inter-column spacing. */
  readonly itemWidth: number;
}

/** Fits one to three equal columns into actual content space without horizontal overflow.
 * Does not choose application navigation or change child identity.
 * @throws RangeError for invalid lengths or a maxColumns value outside 1, 2, 3.
 * @example getGridLayout({ width: 640, minItemWidth: 300, gap: 16 });
 * @example getGridLayout({ width: 280, minItemWidth: 300 }); // one 280-unit column
 */
export function getGridLayout(options: GridLayoutOptions): GridLayout;
