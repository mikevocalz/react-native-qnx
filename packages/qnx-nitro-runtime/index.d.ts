/**
 * Minimal sync QNX-only Nitro v1 proof contract.
 *
 * The C++ HybridClass is registered with NitroModulesProxy by the QNX native
 * host before JS import. This API is NOT a QNX/Marathon generic Nitro runtime.
 * It is a synchronous native binding proof; async callbacks, codegen for QNX,
 * and lifecycle controls are not implemented.
 */
export interface QnxPlatformInfo {
  /** OS identity produced by the native QNX runtime. */
  readonly platform: 'qnx';
  /** CPU architecture reported by the native runtime. */
  readonly architecture: string;
  /** Synchronous deterministic test of the v1 HybridClass binding. */
  add(left: number, right: number): number;
}

/**
 * Create the QNX native binding after installQnxNitroRuntime has run.
 *
 * Throws if the Nitro runtime is missing or the QNX HybridClass is not
 * registered. A Linux/Marathon host must not use this QNX-only constructor.
 */
export function createQnxPlatformInfo(): QnxPlatformInfo;

// ---------------------------------------------------------------------------
// Windows API (S8, gate `window-chrome`), slice 1. See
// docs/nitro/API_CONTRACTS.md for the contract, the close sources and the
// guarantees per platform. Not yet available: `setMaximized`,
// `addOnStateChangedListener` and the separate `QnxTitleBarInset` object.
// ---------------------------------------------------------------------------

/**
 * Multi-window scope: v1 has one window per process. There is one set of
 * guards and listeners, `getQnxWindows()` always returns the same object and it
 * talks to the app's only window. Opening, listing or targeting other windows
 * is outside v1.
 */
export type WindowControlsLayout = 'macos' | 'windows';
/** What this target can do, from the platform capabilities, not from which controls are drawn. */
export type WindowCapability = 'close' | 'minimize' | 'maximize' | 'move' | 'resize';
export type CloseReason = 'control' | 'app' | 'system';

export interface Rect {
  x: number;
  y: number;
  width: number;
  height: number;
}
export interface Size {
  width: number;
  height: number;
}
export interface Point {
  x: number;
  y: number;
}

/** One immutable snapshot of the window: outer size (title bar included), device-independent pixels. */
export interface WindowState {
  /** Increases with every published snapshot. */
  sequence: number;
  title: string;
  size: Size;
  position: Point;
  focused: boolean;
  visible: boolean;
  maximized: boolean;
  controlsLayout: WindowControlsLayout;
}

export interface CloseRequest {
  reason: CloseReason;
  /** False when the source cannot be cancelled: the guard may save, its result is ignored. */
  cancellable: boolean;
  /** Milliseconds left until the process is killed; absent when the source states none (about 3 s under rnqnx-winmgr). */
  deadlineMs?: number;
}

/** Resolve (or return) true to let the window close; anything else keeps it open. */
export type CloseGuard = (request: CloseRequest) => boolean | Promise<boolean>;
/** Call to remove. Calling it again, or after teardown, does nothing. */
export type Unsubscribe = () => void;

export interface CloseOptions {
  /** Skip the guards. Overrides a guarded close that is pending. */
  force?: boolean;
}

export type WindowsErrorCode =
  'WINDOWS_UNAVAILABLE' | 'WINDOW_ACTION_UNSUPPORTED' | 'INVALID_ARGUMENT' | 'WINDOWS_DISPOSED';

/**
 * What the React content must keep clear of, in device-independent pixels
 * relative to the React surface. The bar of the `default` style sits outside
 * the React surface, so it reports 0. In the `hidden` style the controls float
 * over the content: the inset is the strip they sit in and `overlay` is the
 * rectangle they cover. `none` draws nothing and reports 0.
 */
export interface TitleBarInset {
  top: number;
  right: number;
  bottom: number;
  left: number;
  overlay: Rect;
}

export interface QnxWindows {
  /** Final once the window manager has answered, before JavaScript loads. */
  readonly capabilities: readonly WindowCapability[];
  /** The last state the GUI thread published. */
  getState(): WindowState;
  setControlsLayout(layout: WindowControlsLayout): Promise<void>;
  setTitle(title: string): Promise<void>;
  /** Resolves with the rectangle actually applied (clamped to the screen and the minimum size). */
  setBounds(bounds: Rect): Promise<Rect>;
  minimize(): Promise<void>;
  /** Runs the guards. Resolves false if one cancelled; true means the close was accepted. */
  close(options?: CloseOptions): Promise<boolean>;
  addCloseGuard(guard: CloseGuard): Unsubscribe;
  addOnFocusChangedListener(listener: (focused: boolean) => void): Unsubscribe;
  addOnVisibilityChangedListener(listener: (visible: boolean) => void): Unsubscribe;
  /**
   * The title bar inset. Slice 1 serves it from this object; the contract's
   * separate `QnxTitleBarInset` object is the later shape of the same two calls.
   */
  getTitleBarInset(): TitleBarInset;
  /** Coalesced: a burst of layout changes delivers the latest inset once. */
  addOnInsetChangedListener(listener: (inset: TitleBarInset) => void): Unsubscribe;
}

/**
 * The windows API of this process. Rejections and throws carry `error.code`
 * (see `WindowsErrorCode`). Throws `WINDOWS_UNAVAILABLE` when the host did not
 * install window services (built without `RNQNX_ENABLE_WINDOW_CHROME`).
 */
export function getQnxWindows(): QnxWindows;
