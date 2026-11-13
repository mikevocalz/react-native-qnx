// QtVideo with the native control bar and a source switch (bundled clip, a
// stream from the Mac, a bad URL for the error state). The card module is
// required on first render, so a host built without RNQNX_ENABLE_QT_VIDEO
// shows a notice instead of failing at startup.
const React = require('react');
const { Text } = require('react-native');

function VideoAdvancedCard() {
  const UIManager = require('react-native/Libraries/ReactNative/UIManager').default;
  if (!UIManager.hasViewManagerConfig('QnxQtVideo')) {
    return React.createElement(
      Text,
      { style: { color: '#9dafc2' } },
      'QtVideo is not in this build (RNQNX_ENABLE_QT_VIDEO=OFF).'
    );
  }
  const { VideoCard } = require('../../media/VideoCard.js');
  return React.createElement(VideoCard, { width: 600, bare: true });
}

export default {
  id: 'video',
  title: 'Video',
  detail: 'QtVideo: Qt Multimedia (FFmpeg) frames in the Qt Quick scene graph',
  order: 20,
  framed: true,
  Component: VideoAdvancedCard,
};
