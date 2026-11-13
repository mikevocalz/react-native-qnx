const React = require('react');
const { View, Text, Pressable } = require('react-native');
const { QtVideo } = require('../../../packages/react-native-qnx/qt');
const { INK, MUTED, ACCENT, PANEL, SELECTED, DANGER } = require('./theme.js');
const { BUNDLED_TONE } = require('./sources.js');

const h = React.createElement;

function Button({ title, onPress }) {
  return h(
    Pressable,
    {
      onPress, accessibilityRole: 'button', accessibilityLabel: title,
      style: ({ pressed }) => ({
        minHeight: 40, paddingHorizontal: 18, justifyContent: 'center', borderRadius: 9, marginRight: 8,
        backgroundColor: pressed ? '#237a80' : SELECTED,
      }),
    },
    h(Text, { style: { color: '#f2ffff', fontSize: 15 } }, title)
  );
}

/**
 * Audio card: play/pause and volume buttons around a zero-size QtVideo that
 * plays the bundled tone. There is no separate audio component; QtVideo with
 * an audio-only file decodes and plays through the same Qt AudioOutput.
 */
function AudioCard({ width }) {
  const [paused, setPaused] = React.useState(true);
  const [volume, setVolume] = React.useState(0.5);
  const [status, setStatus] = React.useState('idle');
  const ref = React.useRef(null);
  return h(
    View,
    { style: { width, backgroundColor: PANEL, borderRadius: 14, padding: 16, marginBottom: 20 } },
    h(Text, { style: { color: INK, fontSize: 17, marginBottom: 4 } }, 'Audio'),
    h(Text, { style: { color: MUTED, fontSize: 13, marginBottom: 10 } },
      'Qt AudioOutput through io-snd; plays a bundled 3 s tone'),
    h(QtVideo, {
      ref, source: BUNDLED_TONE, paused, volume, repeat: false, style: { width: 1, height: 1 },
      onLoad: (e) => setStatus(`loaded, ${e.duration.toFixed(1)} s`),
      onProgress: (e) => setStatus(`playing ${e.currentTime.toFixed(1)} s`),
      onEnd: () => { setStatus('ended'); setPaused(true); },
      onError: (e) => setStatus(`error: ${e.message}`),
    }),
    h(View, { style: { flexDirection: 'row', marginBottom: 8 } },
      h(Button, { title: paused ? 'Play' : 'Pause', onPress: () => { if (paused && status === 'ended') ref.current.seek(0); setPaused(!paused); } }),
      h(Button, { title: 'Volume -', onPress: () => setVolume(Math.max(0, +(volume - 0.1).toFixed(1))) }),
      h(Button, { title: 'Volume +', onPress: () => setVolume(Math.min(1, +(volume + 0.1).toFixed(1))) })),
    h(Text, { style: { color: status.startsWith('error') ? DANGER : ACCENT, fontSize: 13 }, accessibilityLabel: 'Audio status' },
      `${status}  ·  volume ${Math.round(volume * 100)}%`)
  );
}

module.exports = { AudioCard };
