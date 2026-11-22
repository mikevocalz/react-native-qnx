export interface WindowInsets {
  readonly top: number;
  readonly right: number;
  readonly bottom: number;
  readonly left: number;
}
/** Logical pixels, measured relative to the desktop for x/y. */
export interface WindowSnapshot {
  readonly title: string;
  readonly x: number;
  readonly y: number;
  readonly width: number;
  readonly height: number;
  readonly contentWidth: number;
  readonly contentHeight: number;
  readonly visible: boolean;
  readonly focused: boolean;
  readonly minimized: boolean;
  readonly closeRequested: boolean;
  readonly closeRequestId: string;
  readonly controlsLayout: string;
  readonly insets: WindowInsets;
  readonly canSetPosition: boolean;
  readonly canResize: boolean;
  readonly canPreventClose: boolean;
  readonly canMinimize: boolean;
}
