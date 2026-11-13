import type { CodegenTypes, ColorValue, ViewProps } from 'react-native';
import { codegenNativeComponent } from 'react-native';

type LoadStartEvent = Readonly<{ started: boolean }>;
type ProgressEvent = Readonly<{ loaded: CodegenTypes.Double; total: CodegenTypes.Double }>;
type LoadEvent = Readonly<{
  /** `none` (fetched) or `memory` (served from the decoded cache). */
  cacheType: string;
  source: Readonly<{
    url: string;
    width: CodegenTypes.Double;
    height: CodegenTypes.Double;
    mediaType: string;
    isAnimated: boolean;
  }>;
}>;
type ErrorEvent = Readonly<{ error: string }>;
type DisplayEvent = Readonly<{ displayed: boolean }>;

interface NativeProps extends ViewProps {
  /** expo-image's resolved source array: [{ uri, width, height, headers, cacheKey, ... }]. */
  source?: CodegenTypes.UnsafeMixed;
  placeholder?: CodegenTypes.UnsafeMixed;
  /** How the placeholder fits the view; expo-image's default is `scale-down`. */
  placeholderContentFit?: string;
  responsivePolicy?: string;
  contentFit?: string;
  contentPosition?: CodegenTypes.UnsafeMixed;
  transition?: CodegenTypes.UnsafeMixed;
  tintColor?: ColorValue;
  blurRadius?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  priority?: string;
  cachePolicy?: string;
  recyclingKey?: string;
  allowDownscaling?: CodegenTypes.WithDefault<boolean, true>;
  autoplay?: CodegenTypes.WithDefault<boolean, true>;
  enableLiveTextInteraction?: CodegenTypes.WithDefault<boolean, false>;
  onLoadStart?: CodegenTypes.DirectEventHandler<LoadStartEvent>;
  onProgress?: CodegenTypes.DirectEventHandler<ProgressEvent>;
  onLoad?: CodegenTypes.DirectEventHandler<LoadEvent>;
  onError?: CodegenTypes.DirectEventHandler<ErrorEvent>;
  onDisplay?: CodegenTypes.DirectEventHandler<DisplayEvent>;
}

export default codegenNativeComponent<NativeProps>('ViewManagerAdapter_ExpoImage');
