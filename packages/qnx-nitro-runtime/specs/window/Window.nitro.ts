import { HybridObject } from 'nitro-core';
import type { WindowSnapshot } from './WindowSnapshot';

/** Idempotent cleanup; removing a close guard never closes the window itself. */
export declare class WindowSubscription extends HybridObject {
  remove(): void;
}
/** Independent handle to the app's one native window. Never creates a window. */
export declare class Window extends HybridObject {
  readonly snapshot: WindowSnapshot;
  setTitle(title: string): Promise<void>;
  setSize(width: number, height: number): Promise<void>;
  setPosition(x: number, y: number): Promise<void>;
  setControlsLayout(layout: string): Promise<void>;
  minimize(): Promise<void>;
  restore(): Promise<void>;
  setVisible(visible: boolean): Promise<void>;
  requestFocus(): Promise<void>;
  /** Resolves false when a native close guard vetoes the request. */
  close(): Promise<boolean>;
  /** Cancel the pending close prompt while retaining every native guard. */
  cancelCloseRequest(requestId: string): Promise<boolean>;
  /** Ordered initial snapshot then native changes. Listener exceptions are reported. */
  addSnapshotListener(listener: (snapshot: WindowSnapshot) => void): WindowSubscription;
  /** Native veto remains active while JS is busy. Subscribe to closeRequested for UI. */
  addCloseGuard(): WindowSubscription;
  dispose(): void;
}
/** Installed per runtime by the host; each call returns an independent handle. */
export declare class WindowHost extends HybridObject {
  getCurrentWindow(): Window;
}
