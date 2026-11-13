// Plays a bundled tone through Qt AudioOutput (io-snd on QNX), with
// play/pause and volume. Required lazily, like video.card.js.
const React = require('react');
const { Text } = require('react-native');

function AudioAdvancedCard() {
  const { UIManager } = require('react-native');
  if (!UIManager.hasViewManagerConfig('QnxQtVideo')) {
    return React.createElement(Text, { style: { color: '#9dafc2' } },
      'Audio needs QtVideo, which is not in this build (RNQNX_ENABLE_QT_VIDEO=OFF).');
  }
  const { AudioCard } = require('../../media/AudioCard.js');
  return React.createElement(AudioCard, { width: 600, bare: true });
}

export default {
  id: 'audio',
  title: 'Audio',
  detail: 'Bundled tone through Qt AudioOutput: play, pause, volume',
  order: 21,
  framed: true,
  Component: AudioAdvancedCard,
};
