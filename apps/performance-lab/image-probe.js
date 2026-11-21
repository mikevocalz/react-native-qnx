// Times one remote image load in separate stages. It logs; it asserts nothing.
//   prefetch   Image.prefetch(url): fetch, decode and the completion reaching JS
//   getSizeHit Image.getSize(url) right after prefetch: one JS to native to JS
//              round trip with the decoded image already cached (no fetch, no decode)
//   onLoad     <Image source={{uri}} onLoad> measured from mount, the number
//              RemoteImageCard reports, on a URL no earlier stage touched
// A heartbeat timer records how late the JS task queue drains while a stage
// runs. Date.now() and a monotonic clock are logged together so a wall clock
// step shows up as a difference between them.
// Edit image-probe.config.json to point the url at a server the target can reach.
'use strict';

const React = require('react');
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const View = require('react-native/Libraries/Components/View/View').default;
const Text = require('react-native/Libraries/Text/Text').default;
const Image = require('react-native/Libraries/Image/Image');
const config = require('./image-probe.config.json');

const h = React.createElement;
const monotonic = typeof performance !== 'undefined' && typeof performance.now === 'function';
const mono = () => (monotonic ? performance.now() : Date.now());
const log = (kind, data) => console.log('IMAGE_PROBE ' + kind + ' ' + JSON.stringify(data));

let beat = { last: mono(), maxLateMs: 0, ticks: 0 };
setInterval(() => {
  const now = mono();
  const late = now - beat.last - config.heartbeatMs;
  if (late > beat.maxLateMs) beat.maxLateMs = late;
  beat.last = now;
  beat.ticks += 1;
}, config.heartbeatMs);

function stamp() {
  return { mono: mono(), wall: Date.now() };
}

function elapsed(from) {
  const now = stamp();
  return { monoMs: now.mono - from.mono, wallMs: now.wall - from.wall };
}

function takeBeat() {
  const out = { maxLateMs: beat.maxLateMs, ticks: beat.ticks };
  beat = { last: mono(), maxLateMs: 0, ticks: 0 };
  return out;
}

function withTimeout(promise, label) {
  return Promise.race([
    promise,
    new Promise((_, reject) =>
      setTimeout(() => reject(new Error(label + ' timed out')), config.attemptTimeoutMs)
    ),
  ]);
}

function url(kind, attempt) {
  return config.url + '?probe=' + kind + '-' + attempt;
}

let setView = null;

function viewLoad(attempt) {
  const from = stamp();
  return withTimeout(
    new Promise((resolve, reject) => {
      setView({
        uri: url('view', attempt),
        onLoad: () => resolve(elapsed(from)),
        onError: (event) =>
          reject(
            new Error((event && event.nativeEvent && event.nativeEvent.error) || 'view load failed')
          ),
      });
    }),
    'view load'
  ).finally(() => setView(null));
}

async function runAttempt(attempt) {
  const record = { attempt, clock: monotonic ? 'performance.now' : 'Date.now' };
  takeBeat();
  try {
    let from = stamp();
    await withTimeout(Image.prefetch(url('loader', attempt)), 'prefetch');
    record.prefetch = elapsed(from);
    record.prefetchBeat = takeBeat();
    from = stamp();
    await withTimeout(Image.getSize(url('loader', attempt)), 'getSize');
    record.getSizeHit = elapsed(from);
    record.getSizeHitBeat = takeBeat();
    record.onLoad = await viewLoad(attempt);
    record.onLoadBeat = takeBeat();
  } catch (error) {
    record.error = String(error && error.message ? error.message : error);
  }
  log('attempt', record);
}

function Probe() {
  const [view, setViewState] = React.useState(null);
  setView = setViewState;
  React.useEffect(() => {
    (async () => {
      log('start', { url: config.url, attempts: config.attempts, startedWall: Date.now() });
      for (let attempt = 1; attempt <= config.attempts; attempt++) await runAttempt(attempt);
      log('done', { endedWall: Date.now() });
    })();
  }, []);
  return h(
    View,
    { style: { flex: 1, backgroundColor: '#102033' } },
    h(Text, { style: { color: '#ffffff', fontSize: 18, padding: 12 } }, 'RNQNX image probe'),
    view
      ? h(Image, {
          source: { uri: view.uri },
          onLoad: view.onLoad,
          onError: view.onError,
          style: { width: 160, height: 107 },
        })
      : null
  );
}

AppRegistry.registerComponent('RNQnxPlayground', () => Probe);
