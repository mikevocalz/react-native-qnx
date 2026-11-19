// Proof card for react-native-worklets on the Qt host (stage 2).
//
// It shows, with numbers read back from the device, that a transformed worklet
// runs on the UI-worklet thread and not on the React Native JS thread, that
// frame callbacks arrive from Qt Quick frames, and that jobs already queued on
// the UI thread run while the JS thread is busy.
const React = require('react');
const { View, Text, Pressable } = require('react-native');
const { scheduleOnUI, scheduleOnRN } = require('react-native-worklets');
const { useStore } = require('zustand');
const { createStore } = require('zustand/vanilla');

const h = React.createElement;
const INK = '#e9f0f7';
const MUTED = '#9dafc2';
const ACCENT = '#79e8dc';
const PANEL = '#142538';

function log(line) {
  if (typeof globalThis.__rnqnxLog === 'function') globalThis.__rnqnxLog(line);
}

// Runs on the UI runtime. Reports back through scheduleOnRN.
function startFrameProbe(frames, report) {
  'worklet';
  const role = globalThis.__rnqnxThreadRole();
  const first = Date.now();
  let count = 0;
  let last = 0;
  let worst = 0;
  // Created on the UI runtime: keep this closure local so its counter and
  // recursive reference survive every frame without another serialization.
  function tick(timestamp) {
    count += 1;
    if (last > 0 && timestamp - last > worst) worst = timestamp - last;
    last = timestamp;
    if (count < frames) {
      requestAnimationFrame(tick);
    } else {
      scheduleOnRN(report, {
        role,
        frames: count,
        elapsedMs: Date.now() - first,
        worstFrameMs: Math.round(worst),
        stats: globalThis.__rnqnxFrameStats(),
      });
    }
  }
  requestAnimationFrame(tick);
}

// Queue `jobs` worklets on the UI thread, then block the JS thread. Each worklet
// stamps Date.now(); stamps inside [start, end] ran while JS was blocked.
function jsBusyProbe(busyMs, jobs, report) {
  const stamps = [];
  const record = (stamp) => stamps.push(stamp);
  const start = Date.now();
  for (let i = 0; i < jobs; i += 1) {
    scheduleOnUI(() => {
      'worklet';
      scheduleOnRN(record, Date.now());
    });
  }
  while (Date.now() - start < busyMs) {
    // JS thread busy: this is the load under test.
  }
  const end = Date.now();
  // Stamps are delivered through the JS queue, so they arrive after the block.
  setTimeout(() => {
    const during = stamps.filter((stamp) => stamp >= start && stamp <= end).length;
    report({ jobs, delivered: stamps.length, ranDuringBusy: during, busyMs: end - start });
  }, 50);
}

function WorkletsProof() {
  const store = React.useMemo(() => createStore(() => ({ lines: ['Not run yet.'] })), []);
  const lines = useStore(store, (state) => state.lines);
  const setLines = React.useCallback(
    (next) => {
      store.setState((state) => ({ lines: typeof next === 'function' ? next(state.lines) : next }));
    },
    [store]
  );
  const push = React.useCallback(
    (line) => {
      log(line);
      setLines((previous) => (previous[0] === 'Not run yet.' ? [line] : [...previous, line]));
    },
    [setLines]
  );

  const runAll = React.useCallback(() => {
    setLines([]);
    push('JS thread role: ' + globalThis.__rnqnxThreadRole());
    scheduleOnUI(() => {
      'worklet';
      scheduleOnRN(push, 'worklet thread role: ' + globalThis.__rnqnxThreadRole());
    });
    scheduleOnUI(startFrameProbe, 30, (result) => {
      push(
        'frames ' +
          result.frames +
          ' in ' +
          result.elapsedMs +
          ' ms, worst gap ' +
          result.worstFrameMs +
          ' ms, role ' +
          result.role +
          ', stats ' +
          JSON.stringify(result.stats)
      );
      jsBusyProbe(300, 40, (busy) => {
        push(
          'JS busy ' +
            busy.busyMs +
            ' ms: ' +
            busy.ranDuringBusy +
            ' of ' +
            busy.jobs +
            ' UI worklet jobs ran inside the busy window'
        );
        push('PROOF-COMPLETE');
      });
    });
  }, [push, setLines]);

  return h(
    View,
    { style: { backgroundColor: PANEL, borderRadius: 12, padding: 16, marginTop: 16 } },
    h(
      Text,
      { style: { color: INK, fontSize: 16, marginBottom: 6 } },
      'react-native-worklets on the UI thread'
    ),
    h(
      Pressable,
      {
        onPress: runAll,
        accessibilityLabel: 'Run worklets proof',
        style: {
          backgroundColor: '#1b5b61',
          borderRadius: 8,
          padding: 10,
          alignSelf: 'flex-start',
        },
      },
      h(Text, { style: { color: INK } }, 'Run worklets proof')
    ),
    ...lines.map((line, index) =>
      h(
        Text,
        {
          key: index,
          style: { color: index === lines.length - 1 ? ACCENT : MUTED, fontSize: 13, marginTop: 4 },
        },
        line
      )
    )
  );
}

module.exports = { WorkletsProof };
