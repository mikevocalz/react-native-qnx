// @ts-nocheck: the module loader hook and React stub are deliberately untyped.
// QtVideo JavaScript layer: argument checks of the wrapper, the command path
// through the ref, and the react-native-video API subset. The native view is
// replaced by a recording stub, so this checks the JS contract only; the
// device run is in docs/evidence/media-2026-11-13.md.
import assert from 'node:assert/strict';
import Module, { createRequire } from 'node:module';
import path from 'node:path';

const root = process.cwd();
const require = createRequire(import.meta.url);

const calls = [];
const fakeCommands = {
  seek: (_ref, t) => calls.push(['seek', t]),
  play: () => calls.push(['play']),
  pause: () => calls.push(['pause']),
};
let hasConfig = true;
const rendered = [];
const hooks = { refs: [], handles: [] };
const fakeReact = {
  createElement: (type, props) => {
    rendered.push({ type, props });
    return { type, props };
  },
  forwardRef: (fn) => fn,
  useRef: (initial) => {
    const ref = { current: initial };
    hooks.refs.push(ref);
    return ref;
  },
  useImperativeHandle: (_ref, factory) => hooks.handles.push(factory()),
  useMemo: (fn) => fn(),
  useEffect: () => {},
  useReducer: () => [0, () => {}],
};

const originalLoad = Module._load;
Module._load = function (request, parent, isMain) {
  if (request === 'react') return fakeReact;
  if (request === 'react/jsx-runtime') return fakeReact;
  if (request === 'react-native/Libraries/ReactNative/UIManager') {
    return { default: { hasViewManagerConfig: () => hasConfig } };
  }
  if (request === './QtSvgDocumentNativeComponent') return { default: 'NativeSvg' };
  if (request === './QtVideoNativeComponent') {
    return { default: 'NativeQnxQtVideo', Commands: fakeCommands };
  }
  return originalLoad.call(this, request, parent, isMain);
};

const { QtVideo } = require(path.join(root, 'packages/react-native-qnx/qt/index.js'));

// A string source and a { uri } source both reach the native prop as a string.
QtVideo({ source: 'assets/clip.mp4' });
assert.equal(rendered.at(-1).props.source, 'assets/clip.mp4');
QtVideo({ source: { uri: 'http://host/a.mp4' } });
assert.equal(rendered.at(-1).props.source, 'http://host/a.mp4');

// Events are unwrapped from nativeEvent; onEnd takes no argument.
let loaded;
let ended = 'unset';
QtVideo({ source: 'a', onLoad: (e) => (loaded = e), onEnd: (...a) => (ended = a.length) });
rendered
  .at(-1)
  .props.onVideoLoad({ nativeEvent: { duration: 8, width: 480, height: 270, audioDevice: 'x' } });
assert.deepEqual(loaded, { duration: 8, width: 480, height: 270, audioDevice: 'x' });
rendered.at(-1).props.onVideoEnd({ nativeEvent: { ended: true } });
assert.equal(ended, 0);

// The ref reaches native commands once the native view ref is set.
QtVideo({ source: 'a' });
const handle = hooks.handles.at(-1);
const nativeRef = hooks.refs.at(-1);
handle.seek(5);
assert.equal(calls.length, 0, 'no command before the native view is mounted');
nativeRef.current = {};
handle.seek(5);
handle.play();
handle.pause();
assert.deepEqual(calls, [['seek', 5], ['play'], ['pause']]);

// Bad arguments and a host without the component are rejected.
assert.throws(() => QtVideo({ source: 7 }), /source must be a string/);
assert.throws(() => QtVideo({ source: 'a', resizeMode: 'zoom' }), /resizeMode/);
hasConfig = false;
assert.throws(() => QtVideo({ source: 'a' }), /RNQNX_ENABLE_QT_VIDEO=ON/);
hasConfig = true;

// react-native-video API subset: state, events and the end-of-playback rule.
const { VideoPlayer } = require(path.join(root, 'packages/react-native-qnx/video/index.js'));
const player = new VideoPlayer('http://host/a.mp4');
assert.deepEqual(player.source, { uri: 'http://host/a.mp4' });
assert.equal(player.status, 'loading');
const seen = [];
const sub = player.addEventListener('onVolumeChange', (e) => seen.push(e));
player.volume = 2;
assert.equal(player.volume, 1, 'volume is clamped to 1');
player.muted = true;
assert.deepEqual(seen, [
  { volume: 1, muted: false },
  { volume: 1, muted: true },
]);
sub.remove();
player.muted = false;
assert.equal(seen.length, 2, 'a removed listener is not called');
player.play();
assert.equal(player.isPlaying, false, 'not playing until the media is ready');
console.log('qt-video JavaScript tests passed');
