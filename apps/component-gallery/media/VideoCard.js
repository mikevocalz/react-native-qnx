const React = require('react');
const { View, Text, Pressable } = require('react-native');
const { QtVideo } = require('../../../packages/react-native-qnx/qt');
const { INK, MUTED, ACCENT, PANEL, SELECTED, DANGER } = require('./theme.js');
const { VIDEO_SOURCES } = require('./sources.js');

const h = React.createElement;

function Chip({ title, selected, onPress }) {
  return h(
    Pressable,
    {
      onPress,
      accessibilityRole: 'button',
      accessibilityLabel: title,
      style: {
        minHeight: 34,
        paddingHorizontal: 12,
        justifyContent: 'center',
        marginRight: 6,
        marginBottom: 6,
        borderRadius: 8,
        backgroundColor: selected ? SELECTED : '#0c1929',
      },
    },
    h(Text, { style: { color: selected ? '#f2ffff' : MUTED, fontSize: 14 } }, title)
  );
}

/**
 * Inline QtVideo with the native control bar. The source row switches between
 * the bundled clip, a stream from the Mac and a URL that does not exist (the
 * error state). `fullscreen` fills the window with an absolute-positioned
 * copy of the same view; the toggle is the native bar's own button.
 */
function VideoCard({ width, bare = false }) {
  const [sourceId, setSourceId] = React.useState('bundled');
  const [status, setStatus] = React.useState('idle');
  const [info, setInfo] = React.useState('');
  const [fullscreen, setFullscreen] = React.useState(false);
  const [repeat, setRepeat] = React.useState(false);
  const player = React.useRef(null);
  const source = VIDEO_SOURCES.find((s) => s.id === sourceId);
  const video = h(QtVideo, {
    ref: player,
    key: sourceId,
    source: source.uri,
    controls: true,
    repeat,
    fullscreen,
    resizeMode: 'contain',
    style: fullscreen
      ? {
          position: 'absolute',
          left: 0,
          top: 0,
          right: 0,
          bottom: 0,
          zIndex: 10,
          backgroundColor: '#000',
        }
      : {
          width: '100%',
          height: Math.round((Math.min(width - 32, 640) * 9) / 16),
          backgroundColor: '#000',
        },
    onLoad: (e) => {
      setStatus('loaded');
      setInfo(`${e.width}x${e.height}, ${e.duration.toFixed(1)} s`);
    },
    onProgress: (e) => setStatus(`playing ${e.currentTime.toFixed(1)} s`),
    onEnd: () => setStatus('ended'),
    onError: (e) => setStatus(`error: ${e.message}`),
    onBuffer: (e) => e.isBuffering && setStatus('buffering'),
    onSeek: (e) => setStatus(`seeked to ${e.currentTime.toFixed(1)} s`),
    onFullscreen: (e) => setFullscreen(e.fullscreen),
  });
  return h(
    View,
    {
      style: bare
        ? { width: '100%' }
        : { width, backgroundColor: PANEL, borderRadius: 14, padding: 16, marginBottom: 20 },
    },
    !bare && h(Text, { style: { color: INK, fontSize: 17, marginBottom: 4 } }, 'Video'),
    !bare &&
      h(
        Text,
        { style: { color: MUTED, fontSize: 13, marginBottom: 10 } },
        'QtVideo: Qt Multimedia (FFmpeg) frames in the Qt Quick scene graph'
      ),
    h(
      View,
      { style: { flexDirection: 'row', flexWrap: 'wrap' } },
      ...VIDEO_SOURCES.map((s) =>
        h(Chip, {
          key: s.id,
          title: s.title,
          selected: s.id === sourceId,
          onPress: () => {
            setSourceId(s.id);
            setStatus('idle');
            setInfo('');
          },
        })
      ),
      h(Chip, {
        title: repeat ? 'Repeat on' : 'Repeat off',
        selected: repeat,
        onPress: () => setRepeat(!repeat),
      })
    ),
    video,
    h(
      Text,
      {
        style: { color: status.startsWith('error') ? DANGER : ACCENT, fontSize: 13, marginTop: 8 },
        accessibilityLabel: 'Video status',
      },
      `${status}${info ? '  ·  ' + info : ''}`
    ),
    h(
      View,
      { style: { flexDirection: 'row', marginTop: 8 } },
      h(Chip, { title: 'Seek 5 s', onPress: () => player.current && player.current.seek(5) }),
      h(Chip, { title: 'Pause', onPress: () => player.current && player.current.pause() }),
      h(Chip, { title: 'Play', onPress: () => player.current && player.current.play() })
    )
  );
}

module.exports = { VideoCard };
