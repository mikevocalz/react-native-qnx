import { HybridObject } from 'nitro-core';

/** Device-independent pixels. */
export interface Rect {
  x: number;
  y: number;
  width: number;
  height: number;
}

/**
 * What the React content keeps clear of, in device-independent pixels relative
 * to the React surface. `overlay*` is the rectangle the floating controls cover
 * (width and height 0 when none).
 */
export interface TitleBarInsetData {
  top: number;
  right: number;
  bottom: number;
  left: number;
  overlayX: number;
  overlayY: number;
  overlayWidth: number;
  overlayHeight: number;
}

/** One immutable snapshot of the window: outer size, title bar included. */
export interface WindowStateData {
  sequence: number;
  title: string;
  width: number;
  height: number;
  x: number;
  y: number;
  focused: boolean;
  visible: boolean;
  maximized: boolean;
  controlsLayout: string;
}

/**
 * Native adapter surface of the windows API (docs/nitro/API_CONTRACTS.md, S8).
 * Unions cross as validated strings and listeners as numeric ids; the public
 * contract is built by index.js. Rejections carry `CODE: reason` messages.
 */
export declare class QnxWindows extends HybridObject {
  readonly capabilities: string[];
  getState(): WindowStateData;
  setControlsLayout(layout: string): Promise<void>;
  setTitle(title: string): Promise<void>;
  setBounds(bounds: Rect): Promise<Rect>;
  minimize(): Promise<void>;
  close(force: boolean): Promise<boolean>;
  /** The callback gets (requestId, guardId, reason, cancellable, deadlineMs); -1 means no deadline. */
  addCloseGuard(
    guard: (
      requestId: number,
      guardId: number,
      reason: string,
      cancellable: boolean,
      deadlineMs: number
    ) => void
  ): number;
  /** Answers guard `guardId` for request `requestId`. Ignored when the request is not cancellable. */
  resolveCloseGuard(requestId: number, guardId: number, allow: boolean): void;
  addOnFocusChangedListener(listener: (focused: boolean) => void): number;
  addOnVisibilityChangedListener(listener: (visible: boolean) => void): number;
  getTitleBarInset(): TitleBarInsetData;
  /** The callback gets (top, right, bottom, left, overlayX, overlayY, overlayWidth, overlayHeight). */
  addOnInsetChangedListener(
    listener: (
      top: number,
      right: number,
      bottom: number,
      left: number,
      overlayX: number,
      overlayY: number,
      overlayWidth: number,
      overlayHeight: number
    ) => void
  ): number;
  removeListener(id: number): void;
}
