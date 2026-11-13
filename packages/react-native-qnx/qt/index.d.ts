import type { ReactElement } from 'react';
import type { NativeSyntheticEvent, ViewProps } from 'react-native';

export interface SvgDocumentLoad {
  readonly width: number;
  readonly height: number;
  readonly viewBoxX: number;
  readonly viewBoxY: number;
  readonly viewBoxWidth: number;
  readonly viewBoxHeight: number;
}

export interface QtSvgDocumentProps extends Omit<ViewProps, 'children'> {
  /** Inline UTF-8 SVG XML in the documented static subset. Empty clears. */
  readonly xml: string;
  readonly resizeMode?: 'contain' | 'cover' | 'stretch';
  readonly onDocumentLoad?: (event: NativeSyntheticEvent<SvgDocumentLoad>) => void;
  /** Rejected replacement clears the previous document. */
  readonly onDocumentError?: (event: NativeSyntheticEvent<{ readonly message: string }>) => void;
}

/** Experimental v2 component; requires a host built with Qt SVG enabled. */
export function QtSvgDocument(props: QtSvgDocumentProps): ReactElement;

export interface QtVideoHandle {
  seek(seconds: number): void;
  play(): void;
  pause(): void;
}

export interface QtVideoProps extends Omit<ViewProps, 'children'> {
  /** http(s) URL, file:// URL, absolute path, or a path relative to the executable (bundled assets). */
  readonly source?: string | { readonly uri: string };
  readonly paused?: boolean;
  readonly muted?: boolean;
  /** 0 to 1. */
  readonly volume?: number;
  /** 0.25 to 4. */
  readonly rate?: number;
  readonly repeat?: boolean;
  readonly resizeMode?: 'contain' | 'cover' | 'stretch';
  readonly poster?: string;
  /** Native control bar: play/pause, seek, time, volume, fullscreen. */
  readonly controls?: boolean;
  readonly fullscreen?: boolean;
  readonly onLoad?: (e: { duration: number; width: number; height: number }) => void;
  readonly onProgress?: (e: { currentTime: number; playableDuration: number }) => void;
  readonly onEnd?: () => void;
  readonly onError?: (e: { message: string; code: number }) => void;
  readonly onBuffer?: (e: { isBuffering: boolean }) => void;
  readonly onSeek?: (e: { currentTime: number; seekTime: number }) => void;
  readonly onFullscreen?: (e: { fullscreen: boolean }) => void;
}

/** Requires a host built with RNQNX_ENABLE_QT_VIDEO=ON. */
export const QtVideo: import('react').ForwardRefExoticComponent<
  QtVideoProps & import('react').RefAttributes<QtVideoHandle>
>;
