const React = require('react');
const { ScrollView, View, Text } = require('react-native');
const { VideoCard } = require('./VideoCard.js');
const { AudioCard } = require('./AudioCard.js');
const { INK, MUTED } = require('./theme.js');

const h = React.createElement;

/** The fuller Media screen: video with controls, then audio. */
function MediaPage({ width }) {
  const UIManager = require('react-native/Libraries/ReactNative/UIManager').default;
  if (!UIManager.hasViewManagerConfig('QnxQtVideo')) {
    return h(
      Text,
      { style: { color: MUTED, fontSize: 14 } },
      'QtVideo is not in this build (RNQNX_ENABLE_QT_VIDEO=OFF).'
    );
  }
  const cardWidth = Math.max(320, Math.min(width, 720));
  return h(
    ScrollView,
    { contentContainerStyle: { paddingBottom: 24 } },
    h(
      View,
      { style: { marginBottom: 12 } },
      h(Text, { style: { color: INK, fontSize: 21 } }, 'Media'),
      h(
        Text,
        { style: { color: MUTED, fontSize: 14 } },
        'Video and audio through Qt Multimedia (FFmpeg backend), bundled clip, Mac stream and error state'
      )
    ),
    h(VideoCard, { width: cardWidth }),
    h(AudioCard, { width: cardWidth })
  );
}

module.exports = { MediaPage };
