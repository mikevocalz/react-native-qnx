import type { ReactElement, ReactNode } from 'react';
import type { LayoutChangeEvent, StyleProp, ViewStyle } from 'react-native';

/** Surface and text colors accepted by {@linkcode AdaptiveShell}. */
export interface AdaptiveColors {
  /** Application background. */ readonly background: string;
  /** Navigation and card surface. */ readonly surface: string;
  /** Primary text. */ readonly text: string;
  /** Secondary text. */ readonly muted: string;
  /** Separators and control borders. */ readonly border: string;
  /** Selected navigation surface. */ readonly selected: string;
  /** Text on the selected navigation surface. */ readonly selectedText: string;
  /** Translucent backdrop for the compact navigation dialog. */ readonly backdrop: string;
}

/** Default navy/teal colors used by {@linkcode AdaptiveShell}; callers can supply their own theme. */
export const defaultAdaptiveColors: Readonly<AdaptiveColors>;

/** A stable destination displayed by {@linkcode AdaptiveShell}. */
export interface NavigationItem {
  /** Unique identifier; must not change when the window resizes. */ readonly id: string;
  /** Visible, accessible destination name. */ readonly label: string;
}

/** Props accepted by {@linkcode AdaptiveShell}. */
export interface AdaptiveShellProps {
  /** Application title, always visible. */ readonly title: string;
  /** Optional supporting title, hidden in short windows to reserve content space. */ readonly subtitle?: string;
  /** Destinations available in the sidebar and compact navigation dialog. */ readonly navigation: readonly NavigationItem[];
  /** Currently selected destination, owned by the application. */ readonly selectedId: string;
  /** Receives the selected destination; navigation does not own or reset application state. */ readonly onSelect: (
    id: string
  ) => void;
  /** Application content; the containing native View stays mounted across layout changes. */ readonly children?: ReactNode;
  /** Accessible label on the compact navigation button. @default Sections */ readonly navigationLabel?: string;
  /** Logical width at which a persistent sidebar appears. @default 840 */ readonly sidebarBreakpoint?: number;
  /** Persistent sidebar width in logical units. @default 208 */ readonly sidebarWidth?: number;
  /** Override individual default theme colors. */ readonly colors?: Partial<AdaptiveColors>;
  /** Root styles. */ readonly style?: StyleProp<ViewStyle>;
  /** Main content container styles. */ readonly contentStyle?: StyleProp<ViewStyle>;
  /** Receives the usable content area's actual layout after the sidebar. */ readonly onContentLayout?: (
    event: LayoutChangeEvent
  ) => void;
  /** Identifier of the application root. */ readonly testID?: string;
}

/** Provides a persistent sidebar or an accessible native navigation dialog according to
 * the actual window width. Header density also responds to available height.
 * Geometry and menu state are scoped to this instance. Content never moves into a different parent.
 * @throws RangeError for invalid sidebar dimensions or duplicated/missing navigation IDs.
 */
export function AdaptiveShell(props: AdaptiveShellProps): ReactElement;
