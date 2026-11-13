import type { CodegenTypes, ViewProps } from 'react-native';
import { codegenNativeComponent } from 'react-native';

type RequestChangeEvent = Readonly<{
  /** The state the user is asking for. */
  open: boolean;
  /** `swipe`, `scrim` or `escape`. */
  reason: string;
}>;
type TransitionEndEvent = Readonly<{ open: boolean }>;

interface NativeProps extends ViewProps {
  /** Controlled state. The native view animates to it. */
  open?: CodegenTypes.WithDefault<boolean, false>;
  /** Width in px of the drawer panel (child 0). Style the panel with the same width. */
  drawerWidth?: CodegenTypes.WithDefault<CodegenTypes.Double, 280>;
  side?: CodegenTypes.WithDefault<'left' | 'right', 'left'>;
  /**
   * `front` draws the panel over the content; `slide` moves the content along
   * with it; `back` keeps the panel in place and slides the content away from
   * it. (`permanent` is a plain row layout in JavaScript; it has no native view.)
   */
  drawerType?: CodegenTypes.WithDefault<'front' | 'slide' | 'back', 'front'>;
  swipeEnabled?: CodegenTypes.WithDefault<boolean, true>;
  /** A CSS color string (including rgba()) or a processed color number. */
  scrimColor?: CodegenTypes.UnsafeMixed;
  /** Width in px of the strip at the edge where a drag opens the drawer. */
  edgeWidth?: CodegenTypes.WithDefault<CodegenTypes.Double, 24>;
  /** Left px a swipe cannot start in, for example a navigation rail. */
  swipeExclusionLeft?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  /** Top px the edge strip leaves free, for window controls drawn over the content. */
  topInset?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  /** Only for a change the user asked for; JavaScript then sets `open`. */
  onRequestChange?: CodegenTypes.DirectEventHandler<RequestChangeEvent>;
  /** The drawer reached the open or closed state, for any reason. */
  onTransitionEnd?: CodegenTypes.DirectEventHandler<TransitionEndEvent>;
}

export default codegenNativeComponent<NativeProps>('RNQnxDrawer');
