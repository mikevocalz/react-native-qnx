const React = require('react');
const { useState } = React;
const AppRegistry = require('react-native/Libraries/ReactNative/AppRegistry').AppRegistry;
const { QtSvgDocument } = require('../../packages/react-native-qnx/qt');

globalThis.__qtSvgEvents = [];
globalThis.__qtSvgRejectedProps = 0;
for (const props of [
  { xml: null },
  { xml: '', resizeMode: 'invalid' },
  { xml: '', children: 'invalid' },
]) {
  try {
    QtSvgDocument(props);
  } catch (_) {
    globalThis.__qtSvgRejectedProps++;
  }
}

function SvgFixture() {
  const [state, setState] = useState({
    xml: '<svg xmlns="http://www.w3.org/2000/svg" width="100" height="50" viewBox="0 0 100 50"><rect width="100" height="50" fill="#ff0000"/></svg>',
    width: 100,
    height: 60,
    resizeMode: 'contain',
    visible: true,
  });
  globalThis.__qtSvgUpdate = (patch) => setState((previous) => ({ ...previous, ...patch }));
  if (!state.visible) return null;
  return (
    <QtSvgDocument
      xml={state.xml}
      resizeMode={state.resizeMode}
      style={{ position: 'absolute', left: 4, top: 5, width: state.width, height: state.height }}
      onDocumentLoad={({ nativeEvent }) =>
        globalThis.__qtSvgEvents.push({
          kind: 'load',
          width: nativeEvent.width,
          height: nativeEvent.height,
        })
      }
      onDocumentError={({ nativeEvent }) =>
        globalThis.__qtSvgEvents.push({ kind: 'error', message: nativeEvent.message })
      }
    />
  );
}

AppRegistry.registerComponent('RnqnxQtSvgFixture', () => SvgFixture);
