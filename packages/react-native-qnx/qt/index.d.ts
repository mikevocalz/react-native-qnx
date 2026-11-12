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
