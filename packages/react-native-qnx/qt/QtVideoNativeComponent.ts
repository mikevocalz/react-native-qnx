import type { CodegenTypes, HostComponent, ViewProps } from 'react-native';
import { codegenNativeCommands, codegenNativeComponent } from 'react-native';
import * as React from 'react';

type VideoLoadEvent = Readonly<{
  duration: CodegenTypes.Double;
  width: CodegenTypes.Double;
  height: CodegenTypes.Double;
}>;
type VideoProgressEvent = Readonly<{
  currentTime: CodegenTypes.Double;
  playableDuration: CodegenTypes.Double;
}>;
type VideoEndEvent = Readonly<{ ended: boolean }>;
type VideoErrorEvent = Readonly<{ message: string; code: CodegenTypes.Int32 }>;
type VideoBufferEvent = Readonly<{ isBuffering: boolean }>;
type VideoSeekEvent = Readonly<{
  currentTime: CodegenTypes.Double;
  seekTime: CodegenTypes.Double;
}>;
type VideoFullscreenEvent = Readonly<{ fullscreen: boolean }>;

interface NativeProps extends ViewProps {
  source?: string;
  paused?: CodegenTypes.WithDefault<boolean, false>;
  muted?: CodegenTypes.WithDefault<boolean, false>;
  volume?: CodegenTypes.WithDefault<CodegenTypes.Double, 1>;
  rate?: CodegenTypes.WithDefault<CodegenTypes.Double, 1>;
  repeat?: CodegenTypes.WithDefault<boolean, false>;
  resizeMode?: CodegenTypes.WithDefault<'contain' | 'cover' | 'stretch', 'contain'>;
  poster?: string;
  controls?: CodegenTypes.WithDefault<boolean, false>;
  fullscreen?: CodegenTypes.WithDefault<boolean, false>;
  onVideoLoad?: CodegenTypes.DirectEventHandler<VideoLoadEvent>;
  onVideoProgress?: CodegenTypes.DirectEventHandler<VideoProgressEvent>;
  onVideoEnd?: CodegenTypes.DirectEventHandler<VideoEndEvent>;
  onVideoError?: CodegenTypes.DirectEventHandler<VideoErrorEvent>;
  onVideoBuffer?: CodegenTypes.DirectEventHandler<VideoBufferEvent>;
  onVideoSeek?: CodegenTypes.DirectEventHandler<VideoSeekEvent>;
  onVideoFullscreen?: CodegenTypes.DirectEventHandler<VideoFullscreenEvent>;
}

type ComponentType = HostComponent<NativeProps>;

interface NativeCommands {
  seek: (viewRef: React.ElementRef<ComponentType>, time: CodegenTypes.Double) => void;
  play: (viewRef: React.ElementRef<ComponentType>) => void;
  pause: (viewRef: React.ElementRef<ComponentType>) => void;
}

export const Commands: NativeCommands = codegenNativeCommands<NativeCommands>({
  supportedCommands: ['seek', 'play', 'pause'],
});

export default codegenNativeComponent<NativeProps>('QnxQtVideo');
