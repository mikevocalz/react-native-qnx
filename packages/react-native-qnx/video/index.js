'use strict';
// A JavaScript implementation of the react-native-video 7 player API on top of
// QtVideo (Qt Multimedia). It is not the upstream package: 7.0.0-beta.12 needs
// Nitro v0 (`NitroModules.createHybridObject`, nitrogen 0.35 C++), which the
// pinned Nitro v1 runtime on QNX does not provide. Metro aliases
// `react-native-video` to this file so `useVideoPlayer` and `VideoView` code
// runs unchanged for the subset listed in docs/component-support.md.
const React = require('react');
const { QtVideo } = require('../qt');

const STATUSES = ['idle', 'loading', 'readyToPlay', 'error'];

class VideoPlayer {
  constructor(source) {
    this._source = typeof source === 'string' ? { uri: source } : source || null;
    this._listeners = new Map();
    this._views = new Set();
    this._state = {
      paused: true,
      muted: false,
      volume: 1,
      rate: 1,
      loop: false,
      currentTime: 0,
      duration: NaN,
      status: this._source ? 'loading' : 'idle',
      bufferedPosition: 0,
    };
    this._released = false;
  }
  _emit(name, payload) {
    const set = this._listeners.get(name);
    if (set) for (const cb of [...set]) cb(payload);
  }
  _set(patch) {
    Object.assign(this._state, patch);
    for (const view of this._views) view();
  }
  addEventListener(name, callback) {
    if (!this._listeners.has(name)) this._listeners.set(name, new Set());
    this._listeners.get(name).add(callback);
    return { remove: () => this._listeners.get(name)?.delete(callback) };
  }
  clearAllEvents() {
    this._listeners.clear();
  }
  get source() {
    return this._source;
  }
  get status() {
    return this._state.status;
  }
  get isPlaying() {
    return !this._state.paused && this._state.status === 'readyToPlay';
  }
  get duration() {
    return this._state.duration;
  }
  get currentTime() {
    return this._state.currentTime;
  }
  set currentTime(seconds) {
    this.seekTo(seconds);
  }
  get bufferedPosition() {
    return this._state.bufferedPosition;
  }
  get volume() {
    return this._state.volume;
  }
  set volume(v) {
    this._set({ volume: Math.max(0, Math.min(1, v)) });
    this._emit('onVolumeChange', { volume: this._state.volume, muted: this._state.muted });
  }
  get muted() {
    return this._state.muted;
  }
  set muted(v) {
    this._set({ muted: !!v });
    this._emit('onVolumeChange', { volume: this._state.volume, muted: this._state.muted });
  }
  get loop() {
    return this._state.loop;
  }
  set loop(v) {
    this._set({ loop: !!v });
  }
  get rate() {
    return this._state.rate;
  }
  set rate(v) {
    this._set({ rate: v });
    this._emit('onPlaybackRateChange', { rate: v });
  }
  play() {
    this._set({ paused: false });
    this._emit('onPlaybackStateChange', { isPlaying: true, isBuffering: false });
  }
  pause() {
    this._set({ paused: true });
    this._emit('onPlaybackStateChange', { isPlaying: false, isBuffering: false });
  }
  seekTo(seconds) {
    for (const view of this._seekers || []) view(seconds);
  }
  seekBy(delta) {
    this.seekTo(Math.max(0, this._state.currentTime + delta));
  }
  async replaceSourceAsync(source) {
    this._source = typeof source === 'string' ? { uri: source } : source;
    this._set({ status: source ? 'loading' : 'idle', currentTime: 0, duration: NaN });
  }
  async initialize() {}
  async preload() {}
  release() {
    this._released = true;
    this.clearAllEvents();
    this._views.clear();
  }
}

function useVideoPlayer(source, setup) {
  const key = typeof source === 'string' ? source : source?.uri;
  const player = React.useMemo(() => {
    const created = new VideoPlayer(source);
    if (setup) setup(created);
    return created;
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [key]);
  React.useEffect(() => () => player.release(), [player]);
  return player;
}

function useEvent(player, name, callback) {
  const ref = React.useRef(callback);
  ref.current = callback;
  React.useEffect(() => {
    const sub = player.addEventListener(name, (payload) => ref.current?.(payload));
    return () => sub.remove();
  }, [player, name]);
}

const VideoView = React.forwardRef(function VideoView(props, ref) {
  const {
    player,
    resizeMode,
    controls,
    style,
    onFullscreenChange,
    onPictureInPictureChange,
    ...rest
  } = props;
  const [, rerender] = React.useReducer((n) => n + 1, 0);
  const native = React.useRef(null);
  React.useEffect(() => {
    const redraw = () => rerender();
    const seek = (seconds) => native.current?.seek(seconds);
    player._views.add(redraw);
    (player._seekers = player._seekers || new Set()).add(seek);
    return () => {
      player._views.delete(redraw);
      player._seekers.delete(seek);
    };
  }, [player]);
  React.useImperativeHandle(
    ref,
    () => ({
      enterFullscreen: () => onFullscreenChange?.(true),
      exitFullscreen: () => onFullscreenChange?.(false),
      enterPictureInPicture: () => {},
      exitPictureInPicture: () => {},
    }),
    [onFullscreenChange]
  );
  const s = player._state;
  return React.createElement(QtVideo, {
    ...rest,
    ref: native,
    style,
    source: player.source,
    paused: s.paused,
    muted: s.muted,
    volume: s.volume,
    rate: s.rate,
    repeat: s.loop,
    controls: !!controls,
    resizeMode: resizeMode === 'none' ? 'contain' : resizeMode,
    onLoad: (e) => {
      player._set({ status: 'readyToPlay', duration: e.duration });
      player._emit('onLoad', { duration: e.duration, width: e.width, height: e.height });
      player._emit('onStatusChange', 'readyToPlay');
      player._emit('onReadyToDisplay', undefined);
    },
    onProgress: (e) => {
      player._state.currentTime = e.currentTime;
      player._state.bufferedPosition = e.playableDuration;
      player._emit('onProgress', {
        currentTime: e.currentTime,
        bufferDuration: e.playableDuration,
      });
    },
    onEnd: () => player._emit('onEnd', undefined),
    onError: (e) => {
      player._set({ status: 'error' });
      player._emit('onError', { code: String(e.code), message: e.message });
      player._emit('onStatusChange', 'error');
    },
    onBuffer: (e) => player._emit('onBuffer', e.isBuffering),
    onSeek: (e) => {
      player._state.currentTime = e.currentTime;
      player._emit('onSeek', e.currentTime);
    },
    onFullscreen: (e) => onFullscreenChange?.(e.fullscreen),
  });
});

function setAudioSessionManagementDisabled() {}

module.exports = {
  VideoPlayer,
  VideoView,
  useVideoPlayer,
  useEvent,
  setAudioSessionManagementDisabled,
  STATUSES,
};
