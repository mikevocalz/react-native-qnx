import type { CodegenTypes, ViewProps } from 'react-native';
import { codegenNativeComponent } from 'react-native';

type Tab = Readonly<{
  key: string;
  title: string;
  /** A short glyph (a character or emoji) or an image URI. */
  icon?: string;
  /** Text in the badge; empty or missing hides it. */
  badge?: string;
  accessibilityLabel?: string;
}>;

type TabPressEvent = Readonly<{ key: string }>;
type LayoutModeEvent = Readonly<{
  /** `bottom` or `side`. */
  mode: string;
  /** Height in px for `bottom`, width in px for `side`; 0 when hidden. */
  size: CodegenTypes.Double;
}>;

interface NativeProps extends ViewProps {
  tabs?: ReadonlyArray<Tab>;
  activeKey?: string;
  layout?: CodegenTypes.WithDefault<'auto' | 'bottom' | 'side', 'auto'>;
  sideBreakpoint?: CodegenTypes.WithDefault<CodegenTypes.Double, 700>;
  /** Width in px of the rail in `side` mode, reported in onLayoutMode. */
  railWidth?: CodegenTypes.WithDefault<CodegenTypes.Double, 88>;
  /** Height in px of the bar in `bottom` mode, reported in onLayoutMode. */
  barHeight?: CodegenTypes.WithDefault<CodegenTypes.Double, 60>;
  /** Width the `auto` layout compares to `sideBreakpoint`; 0 uses the parent view's width. */
  availableWidth?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  /** Colors are CSS strings or processed color numbers. */
  tintActive?: CodegenTypes.UnsafeMixed;
  tintInactive?: CodegenTypes.UnsafeMixed;
  badgeColor?: CodegenTypes.UnsafeMixed;
  badgeTextColor?: CodegenTypes.UnsafeMixed;
  dividerColor?: CodegenTypes.UnsafeMixed;
  hidden?: CodegenTypes.WithDefault<boolean, false>;
  onTabPress?: CodegenTypes.DirectEventHandler<TabPressEvent>;
  onTabLongPress?: CodegenTypes.DirectEventHandler<TabPressEvent>;
  onLayoutMode?: CodegenTypes.DirectEventHandler<LayoutModeEvent>;
}

export default codegenNativeComponent<NativeProps>('RNQnxTabBar');
