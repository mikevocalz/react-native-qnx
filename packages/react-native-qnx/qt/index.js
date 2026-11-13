'use strict';

const React = require('react');
const UIManager = require('react-native/Libraries/ReactNative/UIManager').default;
const NativeSvgDocument = require('./QtSvgDocumentNativeComponent').default;

/**
 * Render an inline static SVG document. Requires RNQNX_ENABLE_QT_SVG=ON.
 * Yoga owns the view size; set width and height in style.
 */
function QtSvgDocument(props) {
  if (typeof props.xml !== 'string') {
    throw new TypeError('QtSvgDocument.xml must be an inline SVG string.');
  }
  if (props.resizeMode != null && !['contain', 'cover', 'stretch'].includes(props.resizeMode)) {
    throw new TypeError('QtSvgDocument.resizeMode must be contain, cover, or stretch.');
  }
  if (!UIManager.hasViewManagerConfig('QnxSvgDocument')) {
    throw new Error(
      'QtSvgDocument is unavailable. Build the QNX/Qt host with RNQNX_ENABLE_QT_SVG=ON.'
    );
  }
  if (props.children != null) {
    throw new Error('QtSvgDocument accepts inline XML, not React children.');
  }
  return React.createElement(NativeSvgDocument, { ...props, collapsable: false });
}

const NativeVideo = require('./QtVideoNativeComponent').default;
const VideoCommands = require('./QtVideoNativeComponent').Commands;

const RESIZE_MODES = ['contain', 'cover', 'stretch'];

/**
 * Video playback in the Qt Quick scene graph (Qt Multimedia, FFmpeg backend).
 * Requires a host built with RNQNX_ENABLE_QT_VIDEO=ON and the Qt Multimedia
 * add-on libraries. Events carry the payload directly, not a native event:
 * onLoad({duration,width,height}), onProgress({currentTime,playableDuration}),
 * onEnd(), onError({message,code}), onBuffer({isBuffering}), onSeek({currentTime,seekTime}),
 * onFullscreen({fullscreen}). The ref exposes seek(seconds), play() and pause().
 */
const QtVideo = React.forwardRef(function QtVideo(props, ref) {
  const { source, resizeMode, onLoad, onProgress, onEnd, onError, onBuffer, onSeek, onFullscreen, ...rest } = props;
  if (source != null && typeof source !== 'string' && typeof source?.uri !== 'string') {
    throw new TypeError('QtVideo.source must be a string or { uri: string }.');
  }
  if (resizeMode != null && !RESIZE_MODES.includes(resizeMode)) {
    throw new TypeError('QtVideo.resizeMode must be contain, cover, or stretch.');
  }
  if (!UIManager.hasViewManagerConfig('QnxQtVideo')) {
    throw new Error('QtVideo is unavailable. Build the QNX/Qt host with RNQNX_ENABLE_QT_VIDEO=ON.');
  }
  const nativeRef = React.useRef(null);
  React.useImperativeHandle(ref, () => ({
    seek: (seconds) => nativeRef.current && VideoCommands.seek(nativeRef.current, Number(seconds)),
    play: () => nativeRef.current && VideoCommands.play(nativeRef.current),
    pause: () => nativeRef.current && VideoCommands.pause(nativeRef.current),
  }), []);
  const uri = typeof source === 'string' ? source : source?.uri;
  const wrap = (handler) => (handler ? (event) => handler(event.nativeEvent) : undefined);
  return React.createElement(NativeVideo, {
    ...rest,
    ref: nativeRef,
    source: uri,
    resizeMode,
    collapsable: false,
    onVideoLoad: wrap(onLoad),
    onVideoProgress: wrap(onProgress),
    onVideoEnd: onEnd ? () => onEnd() : undefined,
    onVideoError: wrap(onError),
    onVideoBuffer: wrap(onBuffer),
    onVideoSeek: wrap(onSeek),
    onVideoFullscreen: wrap(onFullscreen),
  });
});

module.exports = { QtSvgDocument, QtVideo };
