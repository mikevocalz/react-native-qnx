/**
 * Device probe for the Nitro windows API (gate `window-chrome`, S8). It logs
 * RNQNX_WINDOWS_PROBE lines and asserts nothing: a failed step is a missing or
 * wrong line, so a receipt quotes the log.
 *
 * What a run shows, in order:
 *   capabilities, state, setTitle, setBounds (when the target can move),
 *   guard registered. Then the driver closes the window twice with the title
 *   bar control (or `rnqnx-winmgr` close): the guard cancels the first request
 *   and allows the second, so the log has `guard request=1 ... decision=cancel`
 *   and `guard request=2 ... decision=allow` and the window ends after it.
 *   `minimize-api` runs after PROBE_MINIMIZE_DELAY_MS, so the driver can watch
 *   the visibility and focus lines (visible=false, then focused=false) and
 *   restore the window with the launcher.
 *
 * Tap the surface to run `close({ force: false })` through the API instead of
 * the control: the guard sees reason=app. The guard runs with no JavaScript
 * block; for the blocked-JS receipt, remove the guard first (tap the label).
 */
const React = require('react');
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const View = require('react-native/Libraries/Components/View/View').default;
const Text = require('react-native/Libraries/Text/Text').default;
const Pressable = require('react-native/Libraries/Components/Pressable/Pressable').default;
const { getQnxWindows } = require('../../packages/qnx-nitro-runtime');

const T0 = Date.now();
const PROBE_MINIMIZE_DELAY_MS = 8000;

function log(message) {
  console.log(`RNQNX_WINDOWS_PROBE ${Date.now() - T0} ${message}`);
}

function describe(error) {
  return `code=${error && error.code} message=${JSON.stringify(error && error.message)}`;
}

function run() {
  let windows;
  try {
    windows = getQnxWindows();
  } catch (error) {
    log(`unavailable ${describe(error)}`);
    return { removeGuard() {}, windows: null };
  }
  log(`capabilities ${JSON.stringify(windows.capabilities)}`);
  const state = windows.getState();
  log(
    `state sequence=${state.sequence} title=${JSON.stringify(state.title)} ` +
      `size=${state.size.width}x${state.size.height} position=${state.position.x},${state.position.y} ` +
      `focused=${state.focused} visible=${state.visible} maximized=${state.maximized} ` +
      `controls=${state.controlsLayout}`
  );

  windows.addOnFocusChangedListener((focused) => log(`focus focused=${focused}`));
  windows.addOnVisibilityChangedListener((visible) => log(`visibility visible=${visible}`));

  // Cancel the first close request, allow the second.
  let requests = 0;
  const removeGuard = windows.addCloseGuard(async (request) => {
    requests += 1;
    const decision = request.cancellable && requests === 1 ? 'cancel' : 'allow';
    log(
      `guard request=${requests} reason=${request.reason} cancellable=${request.cancellable} ` +
        `deadlineMs=${request.deadlineMs === undefined ? 'none' : request.deadlineMs} decision=${decision}`
    );
    return decision === 'allow';
  });
  log('guard registered');

  windows
    .setTitle('Windows API probe')
    .then(() => log(`setTitle ok title=${JSON.stringify(windows.getState().title)}`))
    .catch((error) => log(`setTitle failed ${describe(error)}`));

  if (windows.capabilities.includes('resize') && windows.capabilities.includes('move')) {
    windows
      .setBounds({ x: 60, y: 60, width: 640, height: 480 })
      .then((applied) => log(`setBounds ok applied=${JSON.stringify(applied)}`))
      .catch((error) => log(`setBounds failed ${describe(error)}`));
  } else {
    log('setBounds skipped: target has no move/resize capability');
  }

  // Runtime layout switch: flip the control layout once, 3 s after start.
  setTimeout(() => {
    const next = windows.getState().controlsLayout === 'windows' ? 'macos' : 'windows';
    windows
      .setControlsLayout(next)
      .then(() => log(`setControlsLayout ok layout=${windows.getState().controlsLayout}`))
      .catch((error) => log(`setControlsLayout failed ${describe(error)}`));
  }, 3000);

  setTimeout(() => {
    windows
      .minimize()
      .then(() => log('minimize-api ok'))
      .catch((error) => log(`minimize-api failed ${describe(error)}`));
  }, PROBE_MINIMIZE_DELAY_MS);

  return { removeGuard, windows };
}

function Probe() {
  const handle = React.useRef(null);
  const [guarded, setGuarded] = React.useState(true);
  React.useEffect(() => {
    handle.current = run();
  }, []);
  const closeViaApi = () => {
    if (!handle.current || !handle.current.windows) return;
    handle.current.windows
      .close()
      .then((accepted) => log(`close-api accepted=${accepted}`))
      .catch((error) => log(`close-api failed ${describe(error)}`));
  };
  const dropGuard = () => {
    if (!handle.current) return;
    handle.current.removeGuard();
    setGuarded(false);
    log('guard removed');
  };
  return React.createElement(
    View,
    {
      style: {
        flex: 1,
        backgroundColor: '#102033',
        justifyContent: 'center',
        alignItems: 'center',
      },
    },
    React.createElement(
      Pressable,
      { onPress: closeViaApi, style: { padding: 20, backgroundColor: '#2b6cb0' } },
      React.createElement(
        Text,
        { style: { color: '#fff', fontSize: 22 } },
        'close() through the API'
      )
    ),
    React.createElement(
      Pressable,
      { onPress: dropGuard, style: { marginTop: 20, padding: 20, backgroundColor: '#555' } },
      React.createElement(
        Text,
        { style: { color: '#fff', fontSize: 22 } },
        guarded ? 'Remove the close guard' : 'Guard removed'
      )
    )
  );
}

AppRegistry.registerComponent('RNQnxPlayground', () => Probe);
