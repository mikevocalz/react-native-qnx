import type { CodegenTypes, ColorValue, HostComponent, ViewProps } from 'react-native';
import { codegenNativeCommands, codegenNativeComponent } from 'react-native';
import type * as React from 'react';

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

type NativeType = HostComponent<NativeProps>;

/**
 * What expo-image calls on a view ref. `startAnimating` and `stopAnimating`
 * play or pause an animated image; `lockResource` and `unlockResource` hold or
 * release the loaded picture against `source` changes; `reload` loads the
 * source again and ignores the lock.
 */
interface NativeCommands {
  readonly startAnimating: (viewRef: React.ElementRef<NativeType>) => void;
  readonly stopAnimating: (viewRef: React.ElementRef<NativeType>) => void;
  readonly lockResource: (viewRef: React.ElementRef<NativeType>) => void;
  readonly unlockResource: (viewRef: React.ElementRef<NativeType>) => void;
  readonly reload: (viewRef: React.ElementRef<NativeType>) => void;
}

export const Commands = codegenNativeCommands<NativeCommands>({
  supportedCommands: [
    'startAnimating',
    'stopAnimating',
    'lockResource',
    'unlockResource',
    'reload',
  ],
});

export default codegenNativeComponent<NativeProps>('ViewManagerAdapter_ExpoImage') as NativeType;
