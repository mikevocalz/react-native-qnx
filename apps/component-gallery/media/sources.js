const { Image } = require('react-native');

// Bundled clips are Metro assets; the host resolves the relative path next to
// the executable (the gallery README says where to stage them).
const BUNDLED_VIDEO = Image.resolveAssetSource(require('../assets/media/rnqnx-test-clip.mp4')).uri;
const BUNDLED_TONE = Image.resolveAssetSource(require('../assets/media/rnqnx-test-tone.wav')).uri;

// The Mac stream is served by `python3 -m http.server` in the media directory.
// Set RNQNX_MEDIA_URL at bundle time is not possible, so edit the host here or
// pick the "Mac URL" source and change MAC_HOST below.
const MAC_HOST = '192.168.2.1:8765';

const VIDEO_SOURCES = [
  { id: 'bundled', title: 'Bundled', uri: BUNDLED_VIDEO },
  { id: 'mac', title: 'Mac URL', uri: `http://${MAC_HOST}/rnqnx-test-clip.mp4` },
  { id: 'bad', title: 'Bad URL', uri: 'http://192.168.2.1:9/does-not-exist.mp4' },
];

module.exports = { BUNDLED_VIDEO, BUNDLED_TONE, VIDEO_SOURCES, MAC_HOST };
